// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QStatusBar>

// I wanted a QStatusBar that emits a signal when clicked. Qt only provides event overrides.
class ClickableStatusBar final : public QStatusBar
{
  Q_OBJECT

public:
  explicit ClickableStatusBar(QWidget* parent) : QStatusBar(parent) {}
  ~ClickableStatusBar() override = default;

signals:
  void pressed();

protected:
  void mousePressEvent(QMouseEvent* event) override { emit pressed(); }
};
