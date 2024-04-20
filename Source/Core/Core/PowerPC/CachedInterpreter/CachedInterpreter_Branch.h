// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace PowerPC
{
struct PowerPCState;
}

namespace CachedInstruction
{
struct BranchOperands
{
  u32 origin;
  u32 destination;
};

template <bool update_lr>
s32 Branch(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands);
template <bool update_lr>
s32 BranchToLinkRegister(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands);
template <bool update_lr>
s32 BranchToCountRegister(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands);
}  // namespace CachedInstruction
