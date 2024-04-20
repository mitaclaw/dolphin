// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter_Branch.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace CachedInstruction
{
using AnyCallback = CachedInterpreter::AnyCallback;

template <bool update_lr>
s32 Branch(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands)
{
  ppc_state.npc = operands.destination;
  if constexpr (update_lr)
    LR(ppc_state) = operands.origin + 4;
  return sizeof(AnyCallback) + sizeof(operands);
}

template <bool update_lr>
s32 BranchToLinkRegister(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands)
{
  ppc_state.npc = LR(ppc_state);
  if constexpr (update_lr)
    LR(ppc_state) = operands.origin + 4;
  return sizeof(AnyCallback) + sizeof(operands);
}

template <bool update_lr>
s32 BranchToCountRegister(PowerPC::PowerPCState& ppc_state, const BranchOperands& operands)
{
  ppc_state.npc = CTR(ppc_state);
  if constexpr (update_lr)
    LR(ppc_state) = operands.origin + 4;
  return sizeof(AnyCallback) + sizeof(operands);
}

}  // namespace CachedInstruction

using namespace CachedInstruction;

void CachedInterpreter::bx(UGeckoInstruction inst)
{
  Write(inst.LK ? Branch<true> : Branch<false>, {js.compilerPC, js.op->branchTo});

  if (js.op->branchIsIdleLoop)
    Write(CheckIdle, {m_system.GetCoreTiming(), js.blockStart});
  WriteEndBlock();
}

void CachedInterpreter::bclrx(UGeckoInstruction inst)
{
  if ((inst.BO & 0b10100) == 0b10100)  // 1z1zz - Branch always
  {
    Write(inst.LK ? BranchToLinkRegister<true> : BranchToLinkRegister<false>,
          {js.compilerPC, js.op->branchTo});

    if (js.op->branchIsIdleLoop)
      Write(CheckIdle, {m_system.GetCoreTiming(), js.blockStart});
    WriteEndBlock();
  }
  else
    FallBackToInterpreter(inst);
}

void CachedInterpreter::bcctrx(UGeckoInstruction inst)
{
  if ((inst.BO & 0b10100) == 0b10100)  // 1z1zz - Branch always
  {
    Write(inst.LK ? BranchToCountRegister<true> : BranchToCountRegister<false>,
          {js.compilerPC, js.op->branchTo});

    if (js.op->branchIsIdleLoop)
      Write(CheckIdle, {m_system.GetCoreTiming(), js.blockStart});
    WriteEndBlock();
  }
  else
    FallBackToInterpreter(inst);
}
