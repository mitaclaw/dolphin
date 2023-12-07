// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/BranchWatch.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/MMU.h"

namespace Core
{
void BranchWatch::Clear(const CPUThreadGuard&)
{
  m_selection.clear();
  m_collection_v.clear();
  m_collection_p.clear();
  m_recording_phase = Phase::Blacklist;
  m_blacklist_size = 0;
}

union USnapshotMetadata
{
  using Inspection = BranchWatch::SelectionInspection;
  using StorageType = unsigned long long;

  static_assert(Inspection::EndOfEnumeration == Inspection{(1u << 3) + 1});

  StorageType hex;

  BitField<0, 1, bool, StorageType> is_virtual;
  BitField<1, 1, bool, StorageType> is_selected;
  BitField<2, 4, Inspection, StorageType> inspection;

  USnapshotMetadata() : hex(0) {}
  explicit USnapshotMetadata(bool is_virtual_, bool is_selected_, Inspection inspection_)
      : USnapshotMetadata()
  {
    is_virtual = is_virtual_;
    is_selected = is_selected_;
    inspection = inspection_;
  }
};

void BranchWatch::Save(const CPUThreadGuard& guard, std::FILE* file) const
{
  if (!CanSave())
  {
    ASSERT_MSG(CORE, false, "BranchWatch can not be saved.");
    return;
  }
  if (file == nullptr)
    return;

  for (const Collection::value_type& kv : m_collection_v)
  {
    const auto iter = std::find_if(
        m_selection.begin(), m_selection.end(),
        [&](const Selection::value_type& value) { return value.collection_ptr == &kv; });
    fmt::println(file, "{:08x} {:08x} {:08x} {} {} {:x}", kv.first.origin_addr,
                 kv.first.destin_addr, kv.first.original_inst.hex, kv.second.total_hits,
                 kv.second.hits_snapshot,
                 iter == m_selection.end() ? USnapshotMetadata(true, false, {}).hex :
                                             USnapshotMetadata(true, true, iter->inspection).hex);
  }
  for (const Collection::value_type& kv : m_collection_p)
  {
    const auto iter = std::find_if(
        m_selection.begin(), m_selection.end(),
        [&](const Selection::value_type& value) { return value.collection_ptr == &kv; });
    fmt::println(file, "{:08x} {:08x} {:08x} {} {} {:x}", kv.first.origin_addr,
                 kv.first.destin_addr, kv.first.original_inst.hex, kv.second.total_hits,
                 kv.second.hits_snapshot,
                 iter == m_selection.end() ? USnapshotMetadata(false, false, {}).hex :
                                             USnapshotMetadata(false, true, iter->inspection).hex);
  }
}

void BranchWatch::Load(const CPUThreadGuard& guard, std::FILE* file)
{
  if (file == nullptr)
    return;

  Clear(guard);

  u32 origin_addr, destin_addr, inst_hex;
  std::size_t total_hits, hits_snapshot;
  USnapshotMetadata snapshot_metadata = {};
  while (std::fscanf(file, "%x %x %x %zu %zu %llx", &origin_addr, &destin_addr, &inst_hex,
                     &total_hits, &hits_snapshot, &snapshot_metadata.hex) == 6)
  {
    const bool is_virtual = snapshot_metadata.is_virtual;
    const auto [kv_iter, emplace_success] = [&]() {
      // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
      if (is_virtual)
        return m_collection_v.try_emplace({{origin_addr, destin_addr}, inst_hex},
                                          BranchWatchCollectionValue{total_hits, hits_snapshot});
      return m_collection_p.try_emplace({{origin_addr, destin_addr}, inst_hex},
                                        BranchWatchCollectionValue{total_hits, hits_snapshot});
    }();
    if (emplace_success)
    {
      if (snapshot_metadata.is_selected)
      {
        // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
        m_selection.emplace_back(
            BranchWatchSelectionValueType{&*kv_iter, is_virtual, snapshot_metadata.inspection});
      }
      else if (hits_snapshot != 0)
        ++m_blacklist_size;  // This will be very wrong when not in Blacklist mode. That's ok.
    }
  }
  if (!m_selection.empty())
    m_recording_phase = Phase::Reduction;
}

void BranchWatch::IsolateHasExecuted(const CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    m_selection.reserve(GetCollectionSize() - m_blacklist_size);
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
        m_selection.emplace_back(BranchWatchSelectionValueType{&kv, true, SelectionInspection{}});
        kv.second.hits_snapshot = kv.second.total_hits;
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
        m_selection.emplace_back(BranchWatchSelectionValueType{&kv, false, SelectionInspection{}});
        kv.second.hits_snapshot = kv.second.total_hits;
      }
    m_recording_phase = Phase::Reduction;
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& value) -> bool {
      Collection::value_type* const kv = value.collection_ptr;
      if (kv->second.total_hits == kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateNotExecuted(const CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    for (Collection::value_type& kv : m_collection_v)
      kv.second.hits_snapshot = kv.second.total_hits;
    for (Collection::value_type& kv : m_collection_p)
      kv.second.hits_snapshot = kv.second.total_hits;
    m_blacklist_size = GetCollectionSize();
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& value) -> bool {
      Collection::value_type* const kv = value.collection_ptr;
      if (kv->second.total_hits != kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateWasOverwritten(const CPUThreadGuard& guard)
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    // This is a dirty hack of the assumptions that make the blacklist phase work. If the
    // hits_snapshot is non-zero while in the blacklist phase, that means it has been marked
    // for exclusion from the transition to the reduction phase.
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Virtual);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex == read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Physical);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex == read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& value) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, value.collection_ptr->first.origin_addr,
          value.is_virtual ? PowerPC::RequestedAddressSpace::Virtual :
                             PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return value.collection_ptr->first.original_inst.hex == read_result->value;
    });
    return;
  }
}

void BranchWatch::IsolateNotOverwritten(const CPUThreadGuard& guard)
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    // Same dirty hack with != rather than ==, see above for details
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Virtual);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex != read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Physical);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex != read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& value) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, value.collection_ptr->first.origin_addr,
          value.is_virtual ? PowerPC::RequestedAddressSpace::Virtual :
                             PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return value.collection_ptr->first.original_inst.hex != read_result->value;
    });
    return;
  }
}

void BranchWatch::UpdateHitsSnapshot()
{
  switch (m_recording_phase)
  {
  case Phase::Reduction:
    for (Selection::value_type& value : m_selection)
      value.collection_ptr->second.hits_snapshot = value.collection_ptr->second.total_hits;
    [[fallthrough]];
  case Phase::Blacklist:
    return;
  }
}

void BranchWatch::ClearSelectionInspection()
{
  std::for_each(m_selection.begin(), m_selection.end(),
                [](Selection::value_type& value) { value.inspection = {}; });
}

void BranchWatch::SetSelectedInspected(std::size_t idx, SelectionInspection inspection)
{
  m_selection[idx].inspection |= inspection;
}
}  // namespace Core
