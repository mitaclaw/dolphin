// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchTableView.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QVariant>

#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Debugger/BranchWatchDialog.h"
#include "DolphinQt/Debugger/BranchWatchProxyModel.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"
#include "DolphinQt/Debugger/CodeWidget.h"

BranchWatchProxyModel* BranchWatchTableView::model() const
{
  return static_cast<BranchWatchProxyModel*>(QTableView::model());
}

void BranchWatchTableView::OnClicked(const QModelIndex& index)
{
  const QVariant v = model()->data(index, UserRole::OnClickRole);
  switch (index.column())
  {
  case Column::Symbol:
    if (!v.isValid())
      return;
    [[fallthrough]];
  case Column::Origin:
  case Column::Destination:
    m_code_widget->SetAddress(v.value<u32>(), CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
    return;
  }
}

static bool InstructionSetsLR(UGeckoInstruction inst)
{
  // Assuming that input is sanitized, every branch instruction uses the same LK field.
  return inst.LK;
}

void BranchWatchTableView::OnContextMenu(const QPoint& pos)
{
  if (QModelIndexList index_list = selectionModel()->selectedRows(); index_list.size() > 1)
  {
    QMenu* menu = new QMenu;
    menu->addAction(tr("&Delete All"), [this, index_list = std::move(index_list)]() {
      OnDelete(std::move(index_list));
    });
    menu->exec(QCursor::pos());
  }
  else
  {
    const QModelIndex index = indexAt(pos);
    if (!index.isValid())
      return;
    QMenu* menu = new QMenu;

    menu->addAction(tr("&Delete"), [this, index]() { OnDelete(index); });
    switch (index.column())
    {
    case Column::Origin:
    {
      menu->addAction(tr("Insert &NOP"), [this, index]() { OnSetNOP(index); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized);
      menu->addAction(tr("&Copy Address"), [this, index]() { OnCopyAddress(index); });
      break;
    }
    case Column::Destination:
    {
      const QVariant v =
          model()->data(index.siblingAtColumn(Column::Instruction), UserRole::OnClickRole);
      menu->addAction(tr("Insert &BLR"), [this, index]() { OnSetBLR(index); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       InstructionSetsLR(v.value<u32>()));
      menu->addAction(tr("&Copy Address"), [this, index]() { OnCopyAddress(index); });
      break;
    }
    case Column::Symbol:
    {
      const QVariant v = model()->data(index, UserRole::OnClickRole);
      menu->addAction(tr("Insert &BLR at start"), [this, index]() { OnSetBLR(index); })
          ->setEnabled(Core::GetState() != Core::State::Uninitialized && v.isValid());
      break;
    }
    }
    menu->exec(QCursor::pos());
  }
}

void BranchWatchTableView::OnDelete(const QModelIndex& index)
{
  model()->OnDelete(index);
  m_branch_watch_dialog->UpdateStatus();
}

void BranchWatchTableView::OnDelete(QModelIndexList index_list)
{
  model()->OnDelete(std::move(index_list));
  m_branch_watch_dialog->UpdateStatus();
}

void BranchWatchTableView::OnDeleteKeypress()
{
  OnDelete(selectionModel()->selectedRows());
}

void BranchWatchTableView::OnSetBLR(const QModelIndex& index)
{
  const u32 addr = model()->data(index, UserRole::OnClickRole).value<u32>();
  m_system.GetPowerPC().GetDebugInterface().SetPatch(Core::CPUThreadGuard{m_system}, addr,
                                                     0x4e800020);
  SetInspected(index);
  // TODO: This is not ideal. What I need is a signal for when memory has been changed by the GUI,
  // but I cannot find one. UpdateDisasmDialog comes close, but does too much in one signal. For
  // example, CodeViewWidget will scroll to the current PC when UpdateDisasmDialog is signaled. This
  // seems like a pervasive issue. For example, modifying an instruction in the CodeViewWidget will
  // not reflect in the MemoryViewWidget, and vice versa. Neither of these widgets changing memory
  // will reflect in the JITWidget, either. At the very least, we can make sure the CodeWidget
  // is updated in an acceptable way.
  m_code_widget->Update();
}

void BranchWatchTableView::OnSetNOP(const QModelIndex& index)
{
  const u32 addr = model()->data(index, UserRole::OnClickRole).value<u32>();
  m_system.GetPowerPC().GetDebugInterface().SetPatch(Core::CPUThreadGuard{m_system}, addr,
                                                     0x60000000);
  SetInspected(index);
  // Same issue as OnSetBLR.
  m_code_widget->Update();
}

void BranchWatchTableView::OnCopyAddress(const QModelIndex& index)
{
  const u32 addr = model()->data(index, UserRole::OnClickRole).value<u32>();
  QApplication::clipboard()->setText(QString::number(addr, 16));
}

void BranchWatchTableView::SetInspected(const QModelIndex& index)
{
  model()->SetInspected(index);
}
