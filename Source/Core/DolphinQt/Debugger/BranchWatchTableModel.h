// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractTableModel>
#include <QFont>
#include <QList>
#include <QPair>
#include <QVariant>

#include "Common/SymbolDB.h"

namespace Core
{
class BranchWatch;
class CPUThreadGuard;
class System;
}  // namespace Core
namespace File
{
class IOFile;
}

enum BranchWatchTableModelColumn : int
{
  Instruction = 0,
  Origin,
  Destination,
  RecentHits,
  TotalHits,
  Symbol,
  FinalColumn = Symbol,
  NumberOfColumns,
};

enum BranchWatchTableModelUserRole : int
{
  OnClickRole = Qt::UserRole,
  SortRole,
};

struct BranchWatchTableModelSymbolListValueType
{
  BranchWatchTableModelSymbolListValueType(const Common::Symbol* const origin_symbol,
                                           const Common::Symbol* const destin_symbol)
  {
    if (origin_symbol)
    {
      origin_symbol_name = QString::fromStdString(origin_symbol->name);
      origin_symbol_addr = origin_symbol->address;
    }
    if (destin_symbol)
    {
      destin_symbol_name = QString::fromStdString(destin_symbol->name);
      destin_symbol_addr = destin_symbol->address;
    }
  }

  QVariant origin_symbol_name, origin_symbol_addr;
  QVariant destin_symbol_name, destin_symbol_addr;
};

class BranchWatchTableModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  using Column = BranchWatchTableModelColumn;
  using UserRole = BranchWatchTableModelUserRole;
  using SymbolListValueType = BranchWatchTableModelSymbolListValueType;
  using SymbolList = QList<SymbolListValueType>;

  explicit BranchWatchTableModel(Core::System& system, Core::BranchWatch& branch_watch,
                                 QObject* parent = nullptr)
      : QAbstractTableModel(parent), m_system(system), m_branch_watch(branch_watch)
  {
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
  int columnCount(const QModelIndex& parent = QModelIndex{}) const override;
  bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex{}) override;
  void setFont(const QFont& font) { m_font = font; }

  void OnClearWatch(const Core::CPUThreadGuard& guard);
  void OnBranchHasExecuted(const Core::CPUThreadGuard& guard);
  void OnBranchNotExecuted(const Core::CPUThreadGuard& guard);
  void OnBranchWasOverwritten(const Core::CPUThreadGuard& guard);
  void OnBranchNotOverwritten(const Core::CPUThreadGuard& guard);
  void OnWipeRecentHits();
  void OnWipeInspection();
  void OnDelete(const QModelIndex& index);
  void OnDelete(QModelIndexList index_list);
  void OnToggleDestinationSymbols(bool enabled);

  const QVariant& GetSymbolNameVariant(std::size_t idx, bool destination_symbol) const
  {
    const SymbolListValueType& value = m_symbol_list[idx];
    return destination_symbol ? value.destin_symbol_name : value.origin_symbol_name;
  }
  const QVariant& GetSymbolNameVariant(std::size_t idx) const
  {
    return GetSymbolNameVariant(idx, m_destination_symbols);
  }
  const QVariant& GetSymbolAddrVariant(std::size_t idx, bool destination_symbol) const
  {
    const SymbolListValueType& value = m_symbol_list[idx];
    return destination_symbol ? value.destin_symbol_addr : value.origin_symbol_addr;
  }
  const QVariant& GetSymbolAddrVariant(std::size_t idx) const
  {
    return GetSymbolAddrVariant(idx, m_destination_symbols);
  }

  void Save(const Core::CPUThreadGuard& guard, File::IOFile& file) const;
  void Load(const Core::CPUThreadGuard& guard, File::IOFile& file);
  void UpdateSymbols();
  void UpdateHits();
  void SetInspected(const QModelIndex& index);

private:
  void SetDestinInspected(u32 destin_addr, bool nested);
  void SetSymbolInspected(u32 symbol_addr, bool nested);
  void PrefetchSymbols();

  [[nodiscard]] QVariant DisplayRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant FontRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant TextAlignmentRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant ForegroundRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant OnClickRoleData(const QModelIndex& index) const;
  [[nodiscard]] QVariant SortRoleData(const QModelIndex& index) const;

  Core::System& m_system;
  Core::BranchWatch& m_branch_watch;

  SymbolList m_symbol_list;
  mutable QFont m_font;
  bool m_destination_symbols = false;
};
