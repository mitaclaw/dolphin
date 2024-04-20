// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter_Integer.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Core/PowerPC/PowerPC.h"

namespace CachedInstruction
{
using AnyCallback = CachedInterpreter::AnyCallback;

s32 AddImmediate(PowerPC::PowerPCState& ppc_state, const LoadImmediateOperands& operands)
{
  const auto& [rD, rA, SIMM] = operands;
  ppc_state.gpr[rD] = ppc_state.gpr[rA] + SIMM;
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 LoadImmediate(PowerPC::PowerPCState& ppc_state, const LoadImmediateOperands& operands)
{
  const auto& [rD, rA, SIMM] = operands;
  ppc_state.gpr[rD] = SIMM;
  return sizeof(AnyCallback) + sizeof(operands);
}
}  // namespace CachedInstruction

using namespace CachedInstruction;

void CachedInterpreter::addi(UGeckoInstruction inst)
{
  Write(inst.RA ? AddImmediate : LoadImmediate, {inst.RD, inst.RA, inst.SIMM_16});
}

void CachedInterpreter::addis(UGeckoInstruction inst)
{
  Write(inst.RA ? AddImmediate : LoadImmediate, {inst.RD, inst.RA, inst.SIMM_16 << 16});
}
