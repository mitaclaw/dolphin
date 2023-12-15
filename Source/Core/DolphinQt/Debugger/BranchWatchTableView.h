// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTableView>

#include "Common/CommonTypes.h"

namespace Core
{
class System;
}
class BranchWatchDialog;
class BranchWatchProxyModel;
class CodeWidget;

enum BranchWatchTableModelColumn : int;
enum BranchWatchTableModelUserRole : int;

class BranchWatchTableView final : public QTableView
{
  Q_OBJECT

public:
  using Column = BranchWatchTableModelColumn;
  using UserRole = BranchWatchTableModelUserRole;

  explicit BranchWatchTableView(Core::System& system, BranchWatchDialog* branch_watch_dialog,
                                CodeWidget* code_widget, QWidget* parent = nullptr)
      : QTableView(parent), m_system(system), m_branch_watch_dialog(branch_watch_dialog),
        m_code_widget(code_widget)
  {
  }
  BranchWatchProxyModel* model() const;

  void OnClicked(const QModelIndex& index);
  void OnContextMenu(const QPoint& pos);
  void OnDelete(const QModelIndex& index);
  void OnDelete(QModelIndexList index_list);
  void OnDeleteKeypress();
  void OnSetBLR(const QModelIndex& index);
  void OnSetNOP(const QModelIndex& index);
  void OnCopyAddress(const QModelIndex& index);

  void SetInspected(const QModelIndex& index);

private:
  Core::System& m_system;

  BranchWatchDialog* m_branch_watch_dialog;
  CodeWidget* m_code_widget;
};
