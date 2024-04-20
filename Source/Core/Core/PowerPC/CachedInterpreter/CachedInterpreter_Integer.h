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
struct LoadImmediateOperands
{
  u32 rD : 16;
  u32 rS : 16;
  s32 SIMM;
};

s32 AddImmediate(PowerPC::PowerPCState& ppc_state, const LoadImmediateOperands& operands);
s32 LoadImmediate(PowerPC::PowerPCState& ppc_state, const LoadImmediateOperands& operands);
}  // namespace CachedInstruction
