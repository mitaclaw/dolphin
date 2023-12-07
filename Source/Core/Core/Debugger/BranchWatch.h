// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdio>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"

namespace Core
{
class CPUThreadGuard;
}

namespace Core
{
struct FakeBranchWatchCollectionKey
{
  u32 origin_addr;
  u32 destin_addr;
};
struct BranchWatchCollectionKey : FakeBranchWatchCollectionKey
{
  UGeckoInstruction original_inst;
};
struct BranchWatchCollectionValue
{
  std::size_t total_hits = 0;
  std::size_t hits_snapshot = 0;
};
}  // namespace Core

template <>
struct std::hash<Core::BranchWatchCollectionKey>
{
  std::size_t operator()(const Core::BranchWatchCollectionKey& s) const noexcept
  {
    return std::hash<u64>{}(
        Common::BitCast<u64>(static_cast<const Core::FakeBranchWatchCollectionKey&>(s)));
  }
};

namespace Core
{
inline bool operator==(const BranchWatchCollectionKey& lhs,
                       const BranchWatchCollectionKey& rhs) noexcept
{
  std::hash<BranchWatchCollectionKey> hash;
  return hash(lhs) == hash(rhs) && lhs.original_inst.hex == rhs.original_inst.hex;
}

enum class BranchWatchSelectionInspection : unsigned char
{
  SetOriginNOP = 1u << 0,
  SetDestinBLR = 1u << 1,
  SetOriginSymbolBLR = 1u << 2,
  SetDestinSymbolBLR = 1u << 3,
  EndOfEnumeration,
};

constexpr BranchWatchSelectionInspection operator|(BranchWatchSelectionInspection lhs,
                                                   BranchWatchSelectionInspection rhs)
{
  using underlying_t = std::underlying_type_t<BranchWatchSelectionInspection>;
  return static_cast<BranchWatchSelectionInspection>(static_cast<underlying_t>(lhs) |
                                                     static_cast<underlying_t>(rhs));
}

constexpr BranchWatchSelectionInspection operator&(BranchWatchSelectionInspection lhs,
                                                   BranchWatchSelectionInspection rhs)
{
  using underlying_t = std::underlying_type_t<BranchWatchSelectionInspection>;
  return static_cast<BranchWatchSelectionInspection>(static_cast<underlying_t>(lhs) &
                                                     static_cast<underlying_t>(rhs));
}

constexpr BranchWatchSelectionInspection& operator|=(BranchWatchSelectionInspection& self,
                                                     BranchWatchSelectionInspection other)
{
  return self = self | other;
}

class BranchWatchCollection final
    : public std::unordered_map<BranchWatchCollectionKey, BranchWatchCollectionValue>
{
};

struct BranchWatchSelectionValueType
{
  using Inspection = BranchWatchSelectionInspection;

  BranchWatchCollection::value_type* collection_ptr;
  bool is_virtual;
  // This is moreso a GUI thing, but it works best in the Core code for multiple reasons.
  Inspection inspection;
};

class BranchWatchSelection final : public std::vector<BranchWatchSelectionValueType>
{
};

enum class BranchWatchPhase : bool
{
  Blacklist,
  Reduction,
};

class BranchWatch final  // Class is final to enforce the safety of GetOffsetOfRecordingActive().
{
public:
  using Collection = BranchWatchCollection;
  using Selection = BranchWatchSelection;
  using Phase = BranchWatchPhase;
  using SelectionInspection = BranchWatchSelectionInspection;

  void Start() { m_recording_active = true; }
  void Pause() { m_recording_active = false; }
  void Clear(const CPUThreadGuard& guard);

  void Save(const CPUThreadGuard& guard, std::FILE* file) const;
  void Load(const CPUThreadGuard& guard, std::FILE* file);

  void IsolateHasExecuted(const CPUThreadGuard& guard);
  void IsolateNotExecuted(const CPUThreadGuard& guard);
  void IsolateWasOverwritten(const CPUThreadGuard& guard);
  void IsolateNotOverwritten(const CPUThreadGuard& guard);
  void UpdateHitsSnapshot();
  void ClearSelectionInspection();
  void SetSelectedInspected(std::size_t idx, SelectionInspection inspection);

  Selection& GetSelection() { return m_selection; }
  const Selection& GetSelection() const { return m_selection; }

  std::size_t GetCollectionSize() const { return m_collection_v.size() + m_collection_p.size(); }
  std::size_t GetBlacklistSize() const { return m_blacklist_size; }
  Phase GetRecordingPhase() const { return m_recording_phase; };
  bool GetRecordingActive() const { return m_recording_active; }

  // An empty selection in reduction mode can't be reconstructed when loading from a file.
  bool CanSave() const { return !(m_recording_phase == Phase::Reduction && m_selection.empty()); }

  // All Hit member functions are for the CPUThread only
  // HitX_fk are optimized for when origin and destination can be passed in one register easily.
  // HitX_fk_n are the same, but also increment the total_hits by N (see dcbx JIT code).
  static void HitV_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_v[{Common::BitCast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitP_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_p[{Common::BitCast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitV_fk_n(BranchWatch* branch_watch, u64 fake_key, u32 inst, u32 n)
  {
    branch_watch->m_collection_v[{Common::BitCast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += n;
  }

  static void HitP_fk_n(BranchWatch* branch_watch, u64 fake_key, u32 inst, u32 n)
  {
    branch_watch->m_collection_p[{Common::BitCast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += n;
  }

  static void HitV(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitV_fk(branch_watch, Common::BitCast<u64>(FakeBranchWatchCollectionKey{origin, destination}),
            inst);
  }

  static void HitP(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitP_fk(branch_watch, Common::BitCast<u64>(FakeBranchWatchCollectionKey{origin, destination}),
            inst);
  }

  void Hit(u32 origin, u32 destination, UGeckoInstruction inst, bool translate)
  {
    if (translate)
      HitV(this, origin, destination, inst.hex);
    else
      HitP(this, origin, destination, inst.hex);
  }

  // The JIT needs this value, but doesn't need to be a full-on friend.
  static constexpr std::size_t GetOffsetOfRecordingActive()
  {
    return offsetof(BranchWatch, m_recording_active);
  }

private:
  std::size_t m_blacklist_size = 0;
  Phase m_recording_phase = Phase::Blacklist;
  bool m_recording_active = false;
  Collection m_collection_v;  // Virtual memory
  Collection m_collection_p;  // Physical memory
  Selection m_selection;
};

#if _M_X86_64
static_assert(BranchWatch::GetOffsetOfRecordingActive() < 0x80);  // Makes JIT code smaller.
#endif
}  // namespace Core
