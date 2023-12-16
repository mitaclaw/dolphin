// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchTableModel.h"

#include <algorithm>
#include <array>

#include <QBrush>
#include <QFont>

#include "Common/GekkoDisassembler.h"
#include "Common/IOFile.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/PPCSymbolDB.h"

QVariant BranchWatchTableModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  switch (role)
  {
  case Qt::DisplayRole:
    return DisplayRoleData(index);
  case Qt::FontRole:
    return FontRoleData(index);
  case Qt::TextAlignmentRole:
    return TextAlignmentRoleData(index);
  case Qt::ForegroundRole:
    return ForegroundRoleData(index);
  case UserRole::OnClickRole:
    return OnClickRoleData(index);
  case UserRole::SortRole:
    return SortRoleData(index);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Vertical || role != Qt::DisplayRole)
    return QVariant();

  if (section == Column::Symbol)
    return m_destination_symbols ? tr("Destination Symbol") : tr("Origin Symbol");

  static const std::array<QString, Column::NumberOfColumns> headers = {
      tr("Instr."),      tr("Origin"),     tr("Destination"),
      tr("Recent Hits"), tr("Total Hits"), tr("Symbol")};
  return headers[section];
}

int BranchWatchTableModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return static_cast<int>(m_branch_watch.GetSelection().size());
}

int BranchWatchTableModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return Column::NumberOfColumns;
}

bool BranchWatchTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
  if (parent.isValid() || row < 0)
    return false;
  if (count <= 0)
    return true;

  auto& selection = m_branch_watch.GetSelection();
  beginRemoveRows(parent, row, row + count - 1);  // Last is inclusive in Qt!
  selection.erase(selection.begin() + row, selection.begin() + row + count);
  m_symbol_list.remove(row, count);
  endRemoveRows();
  return true;
}

void BranchWatchTableModel::OnClearWatch(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.Clear(guard);
  m_symbol_list.clear();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchHasExecuted(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateHasExecuted(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchNotExecuted(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateNotExecuted(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchWasOverwritten(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateWasOverwritten(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnBranchNotOverwritten(const Core::CPUThreadGuard& guard)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.IsolateNotOverwritten(guard);
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::OnWipeRecentHits()
{
  const int row_count = rowCount();
  if (row_count == 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  m_branch_watch.UpdateHitsSnapshot();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::RecentHits), createIndex(last, Column::RecentHits),
                   roles);
}

void BranchWatchTableModel::OnWipeInspection()
{
  const int row_count = rowCount();
  if (row_count == 0)
    return;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};
  m_branch_watch.ClearSelectionInspection();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::Origin), createIndex(last, Column::Destination), roles);
  emit dataChanged(createIndex(0, Column::Symbol), createIndex(last, Column::Symbol), roles);
}

void BranchWatchTableModel::OnDelete(const QModelIndex& index)
{
  if (!index.isValid())
    return;
  removeRow(index.row());
}

void BranchWatchTableModel::OnDelete(QModelIndexList index_list)
{
  std::sort(index_list.begin(), index_list.end());
  // TODO C++20: std::ranges::reverse_view
  for (auto iter = index_list.rbegin(); iter != index_list.rend(); ++iter)
    OnDelete(*iter);
}

void BranchWatchTableModel::OnToggleDestinationSymbols(bool enabled)
{
  const int row_count = rowCount();
  if (row_count == 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole, Qt::FontRole, Qt::ForegroundRole};
  m_destination_symbols = enabled;
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::Symbol), createIndex(last, Column::Symbol), roles);
  emit headerDataChanged(Qt::Horizontal, Column::Symbol, Column::Symbol);
}

void BranchWatchTableModel::Save(const Core::CPUThreadGuard& guard, File::IOFile& file) const
{
  m_branch_watch.Save(guard, file.GetHandle());
}

void BranchWatchTableModel::Load(const Core::CPUThreadGuard& guard, File::IOFile& file)
{
  emit layoutAboutToBeChanged();
  m_branch_watch.Load(guard, file.GetHandle());
  PrefetchSymbols();
  emit layoutChanged();
}

void BranchWatchTableModel::UpdateSymbols()
{
  const int row_count = rowCount();
  if (row_count == 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  PrefetchSymbols();
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::Symbol), createIndex(last, Column::Symbol), roles);
}

void BranchWatchTableModel::UpdateHits()
{
  const int row_count = rowCount();
  if (row_count == 0)
    return;
  static const QList<int> roles = {Qt::DisplayRole};
  const int last = row_count - 1;
  emit dataChanged(createIndex(0, Column::RecentHits), createIndex(last, Column::TotalHits), roles);
}

void BranchWatchTableModel::SetInspected(const QModelIndex& index)
{
  switch (index.column())
  {
  case Column::Origin:
  {
    using Inspection = Core::BranchWatchSelectionInspection;
    static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};
    m_branch_watch.SetSelectedInspected(index.row(), Inspection::SetOriginNOP);
    emit dataChanged(index, index, roles);
    return;
  }
  case Column::Destination:
  {
    const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
    const u32 destin_addr = selection[index.row()].collection_ptr->first.destin_addr;
    SetDestinInspected(destin_addr, false);
    return;
  }
  case Column::Symbol:
  {
    const u32 symbol_addr = GetSymbolAddrVariant(index.row()).value<u32>();
    SetSymbolInspected(symbol_addr, false);
    return;
  }
  }
}

void BranchWatchTableModel::SetDestinInspected(u32 destin_addr, bool nested)
{
  using Inspection = Core::BranchWatchSelectionInspection;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};

  const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
  for (std::size_t i = 0; i < selection.size(); ++i)
  {
    if (selection[i].collection_ptr->first.destin_addr != destin_addr)
      continue;
    m_branch_watch.SetSelectedInspected(i, Inspection::SetDestinBLR);
    const QModelIndex index = createIndex(static_cast<int>(i), Column::Destination);
    emit dataChanged(index, index, roles);
  }

  if (nested)
    return;
  SetSymbolInspected(destin_addr, true);
}

void BranchWatchTableModel::SetSymbolInspected(u32 symbol_addr, bool nested)
{
  using Inspection = Core::BranchWatchSelectionInspection;
  static const QList<int> roles = {Qt::FontRole, Qt::ForegroundRole};

  for (qsizetype i = 0; i < m_symbol_list.size(); ++i)
  {
    if (const QVariant symbol_addr_v = m_symbol_list[i].origin_symbol_addr;
        symbol_addr_v.isValid() && symbol_addr == symbol_addr_v.value<u32>())
    {
      m_branch_watch.SetSelectedInspected(i, Inspection::SetOriginSymbolBLR);
      if (m_destination_symbols == false)  // Hopefully the branch predictor helps...
      {
        const QModelIndex index = createIndex(i, Column::Symbol);
        emit dataChanged(index, index, roles);
      }
    }
    if (const QVariant symbol_addr_v = m_symbol_list[i].destin_symbol_addr;
        symbol_addr_v.isValid() && symbol_addr == symbol_addr_v.value<u32>())
    {
      m_branch_watch.SetSelectedInspected(i, Inspection::SetDestinSymbolBLR);
      if (m_destination_symbols == true)  // Hopefully the branch predictor helps...
      {
        const QModelIndex index = createIndex(i, Column::Symbol);
        emit dataChanged(index, index, roles);
      }
    }
  }

  if (nested)
    return;
  SetDestinInspected(symbol_addr, true);
}

void BranchWatchTableModel::PrefetchSymbols()
{
  if (m_branch_watch.GetRecordingPhase() != Core::BranchWatch::Phase::Reduction)
    return;

  const Core::BranchWatch::Selection& selection = m_branch_watch.GetSelection();
  m_symbol_list.clear();
  m_symbol_list.reserve(selection.size());
  for (const Core::BranchWatch::Selection::value_type& value : selection)
  {
    const Core::BranchWatch::Collection::value_type* const kv = value.collection_ptr;
    m_symbol_list.emplace_back(g_symbolDB.GetSymbolFromAddr(kv->first.origin_addr),
                               g_symbolDB.GetSymbolFromAddr(kv->first.destin_addr));
  }
}

static QString GetInstructionMnemonic(u32 hex)
{
  const std::string disas = Common::GekkoDisassembler::Disassemble(hex, 0);
  const auto split = disas.find('\t');
  // I wish I could disassemble just the mnemonic!
  if (split == std::string::npos)
    return QString::fromStdString(disas);
  return QString::fromLatin1(disas.data(), split);
}

QVariant BranchWatchTableModel::DisplayRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
  {
    if (const QVariant& symbol_name_v = GetSymbolNameVariant(index.row()); symbol_name_v.isValid())
      return symbol_name_v;
    return QStringLiteral(" --- ");
  }

  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].collection_ptr;
  switch (index.column())
  {
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Origin:
    return QString::number(kv->first.origin_addr, 16);
  case Column::Destination:
    return QString::number(kv->first.destin_addr, 16);
  case Column::RecentHits:
    return QString::number(kv->second.total_hits - kv->second.hits_snapshot);
  case Column::TotalHits:
    return QString::number(kv->second.total_hits);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::FontRoleData(const QModelIndex& index) const
{
  m_font.setBold([&]() -> bool {
    using Inspection = Core::BranchWatchSelectionInspection;
    switch (index.column())
    {
    case Column::Origin:
    {
      const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
      return (inspection & Inspection::SetOriginNOP) != 0;
    }
    case Column::Destination:
    {
      const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
      return (inspection & Inspection::SetDestinBLR) != 0;
    }
    case Column::Symbol:
    {
      const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
      const Inspection inspection_mask =
          m_destination_symbols ? Inspection::SetDestinSymbolBLR : Inspection::SetOriginSymbolBLR;
      return (inspection & inspection_mask) != 0;
    }
    }
    // Importantly, this code path avoids subscripting the selection to get an inspection value.
    return false;
  }());
  return m_font;
}

QVariant BranchWatchTableModel::TextAlignmentRoleData(const QModelIndex& index) const
{
  // Qt enums become QFlags when operators are used. QVariant's constructors don't support QFlags.
  switch (index.column())
  {
  case Column::Origin:
  case Column::Destination:
    return Qt::AlignCenter;
  case Column::RecentHits:
  case Column::TotalHits:
    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
  case Column::Instruction:
  case Column::Symbol:
    return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
  }
  return QVariant();
}

QVariant BranchWatchTableModel::ForegroundRoleData(const QModelIndex& index) const
{
  switch (index.column())
  {
    using Inspection = Core::BranchWatchSelectionInspection;
  case Column::Origin:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetOriginNOP) != 0 ? QBrush(Qt::red) : QVariant();
  }
  case Column::Destination:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    return (inspection & Inspection::SetDestinBLR) != 0 ? QBrush(Qt::red) : QVariant();
  }
  case Column::Symbol:
  {
    const Inspection inspection = m_branch_watch.GetSelection()[index.row()].inspection;
    const Inspection inspection_mask =
        m_destination_symbols ? Inspection::SetDestinSymbolBLR : Inspection::SetOriginSymbolBLR;
    return (inspection & inspection_mask) != 0 ? QBrush(Qt::red) : QVariant();
  }
  }
  // Importantly, this code path avoids subscripting the selection to get an inspection value.
  return QVariant();
}

QVariant BranchWatchTableModel::OnClickRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
    return GetSymbolAddrVariant(index.row());

  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].collection_ptr;
  switch (index.column())
  {
  case Column::Instruction:
    return kv->first.original_inst.hex;
  case Column::Origin:
    return kv->first.origin_addr;
  case Column::Destination:
    return kv->first.destin_addr;
  }
  return QVariant();
}

QVariant BranchWatchTableModel::SortRoleData(const QModelIndex& index) const
{
  if (index.column() == Column::Symbol)
    return GetSymbolNameVariant(index.row());

  const Core::BranchWatch::Collection::value_type* kv =
      m_branch_watch.GetSelection()[index.row()].collection_ptr;
  switch (index.column())
  {
  // QVariant's ctor only supports (unsigned) int and (unsigned) long long for some stupid reason.
  // std::size_t is unsigned long on some platforms, which results in an ambiguous conversion.
  case Column::Instruction:
    return GetInstructionMnemonic(kv->first.original_inst.hex);
  case Column::Origin:
    return kv->first.origin_addr;
  case Column::Destination:
    return kv->first.destin_addr;
  case Column::RecentHits:
    return qulonglong{kv->second.total_hits - kv->second.hits_snapshot};
  case Column::TotalHits:
    return qulonglong{kv->second.total_hits};
  }
  return QVariant();
}
