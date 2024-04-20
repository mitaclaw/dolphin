// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter_LoadStore.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/Assert.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace CachedInstruction
{
using AnyCallback = CachedInterpreter::AnyCallback;

// You must pass the operands by reference instead of the relevant members from the operands,
// or else the compiler will do some inefficient things when inlining this function.
template <LoadScalars T, bool with_update, class Operands>
static s32 MMULoad(PowerPC::PowerPCState& ppc_state, const Operands& operands, u32 EA)
{
  const T temp = operands.mmu.template Read<T>(EA);
  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    ppc_state.pc = operands.current_pc;
    ppc_state.downcount -= operands.downcount;
    operands.power_pc.CheckExceptions();
    return 0;
  }
  ppc_state.gpr[operands.reg] = temp;
  if constexpr (with_update)
    ppc_state.gpr[operands.rA] = EA;
  return sizeof(AnyCallback) + sizeof(operands);
}

template <LoadScalars T, bool with_update, bool absolute>
s32 LoadAndZero(PowerPC::PowerPCState& ppc_state, const LoadStoreOperands& operands)
{
  const u32 EA = operands.SIMM + (absolute ? 0 : ppc_state.gpr[operands.rA]);
  return MMULoad<T, with_update>(ppc_state, operands, EA);
}

template <LoadScalars T, bool with_update, bool absolute>
s32 LoadAndZeroIndexed(PowerPC::PowerPCState& ppc_state, const LoadStoreIndexedOperands& operands)
{
  const u32 EA = ppc_state.gpr[operands.rB] + (absolute ? 0 : ppc_state.gpr[operands.rA]);
  return MMULoad<T, with_update>(ppc_state, operands, EA);
}

template <StoreScalars T, bool with_update, class Operands>
static s32 MMUStore(PowerPC::PowerPCState& ppc_state, const Operands& operands, u32 EA)
{
  operands.mmu.template Write<T>(ppc_state.gpr[operands.reg], EA);
  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    ppc_state.pc = operands.current_pc;
    ppc_state.downcount -= operands.downcount;
    operands.power_pc.CheckExceptions();
    return 0;
  }
  if constexpr (with_update)
    ppc_state.gpr[operands.rA] = EA;
  return sizeof(AnyCallback) + sizeof(operands);
}

template <StoreScalars T, bool with_update, bool absolute>
s32 Store(PowerPC::PowerPCState& ppc_state, const LoadStoreOperands& operands)
{
  const u32 EA = operands.SIMM + (absolute ? 0 : ppc_state.gpr[operands.rA]);
  return MMUStore<T, with_update>(ppc_state, operands, EA);
}

template <StoreScalars T, bool with_update, bool absolute>
s32 StoreIndexed(PowerPC::PowerPCState& ppc_state, const LoadStoreIndexedOperands& operands)
{
  const u32 EA = ppc_state.gpr[operands.rB] + (absolute ? 0 : ppc_state.gpr[operands.rA]);
  return MMUStore<T, with_update>(ppc_state, operands, EA);
}
}  // namespace CachedInstruction

using namespace CachedInstruction;

template <typename T, bool with_update>
void CachedInterpreter::lXX_or_lXXu(UGeckoInstruction inst)
{
  INSTRUCTION_START;
  JITDISABLE(bJITLoadStoreOff);
  if constexpr (Common::SameAsAnyOf<T, u32, u8, u16>)  // But not lha(u)?
    JITDISABLE(bJITLoadStorelXzOff);
  if constexpr (Common::SameAsAnyOf<T, u32>)
    JITDISABLE(bJITLoadStorelwzOff);
  Write(inst.RA ? LoadAndZero<T, with_update, false> : LoadAndZero<T, with_update, true>,
        {{m_system.GetPowerPC(), js.compilerPC, js.downcountAmount},
         m_mmu,
         inst.RD,
         inst.RA,
         inst.SIMM_16});
}

template <typename T, bool with_update>
void CachedInterpreter::lXXx_or_lXXux(UGeckoInstruction inst)
{
  INSTRUCTION_START;
  JITDISABLE(bJITLoadStoreOff);
  if constexpr (Common::SameAsAnyOf<T, u32, u8, u16>)  // But not lha(u)x?
    JITDISABLE(bJITLoadStorelXzOff);
  if constexpr (Common::SameAsAnyOf<T, u32>)
    JITDISABLE(bJITLoadStorelwzOff);
  if constexpr (Common::SameAsAnyOf<T, u8>)
    JITDISABLE(bJITLoadStorelbzxOff);
  Write(inst.RA ? LoadAndZeroIndexed<T, with_update, false> :
                  LoadAndZeroIndexed<T, with_update, true>,
        {{m_system.GetPowerPC(), js.compilerPC, js.downcountAmount},
         m_mmu,
         inst.RD,
         inst.RA,
         inst.RB});
}

template <typename T, bool with_update>
void CachedInterpreter::stX_or_stXu(UGeckoInstruction inst)
{
  INSTRUCTION_START;
  JITDISABLE(bJITLoadStoreOff);
  Write(inst.RA ? Store<T, with_update, false> : Store<T, with_update, true>,
        {{m_system.GetPowerPC(), js.compilerPC, js.downcountAmount},
         m_mmu,
         inst.RS,
         inst.RA,
         inst.SIMM_16});
}

template <typename T, bool with_update>
void CachedInterpreter::stXx_or_stXux(UGeckoInstruction inst)
{
  INSTRUCTION_START;
  JITDISABLE(bJITLoadStoreOff);
  Write(inst.RA ? StoreIndexed<T, with_update, false> : StoreIndexed<T, with_update, true>,
        {{m_system.GetPowerPC(), js.compilerPC, js.downcountAmount},
         m_mmu,
         inst.RS,
         inst.RA,
         inst.RB});
}

template void CachedInterpreter::lXX_or_lXXu<u32, false>(UGeckoInstruction);  // lwz
template void CachedInterpreter::lXX_or_lXXu<u32, true>(UGeckoInstruction);   // lwzu
template void CachedInterpreter::lXX_or_lXXu<u8, false>(UGeckoInstruction);   // lbz
template void CachedInterpreter::lXX_or_lXXu<u8, true>(UGeckoInstruction);    // lbzu
template void CachedInterpreter::lXX_or_lXXu<u16, false>(UGeckoInstruction);  // lhz
template void CachedInterpreter::lXX_or_lXXu<u16, true>(UGeckoInstruction);   // lhzu
// template void CachedInterpreter::lXX_or_lXXu<s16, false>(UGeckoInstruction);  // lha
// template void CachedInterpreter::lXX_or_lXXu<s16, true>(UGeckoInstruction);   // lhau

template void CachedInterpreter::lXXx_or_lXXux<u32, false>(UGeckoInstruction);  // lwzx
template void CachedInterpreter::lXXx_or_lXXux<u32, true>(UGeckoInstruction);   // lwzux
template void CachedInterpreter::lXXx_or_lXXux<u8, false>(UGeckoInstruction);   // lbzx
template void CachedInterpreter::lXXx_or_lXXux<u8, true>(UGeckoInstruction);    // lbzux
template void CachedInterpreter::lXXx_or_lXXux<u16, false>(UGeckoInstruction);  // lhzx
template void CachedInterpreter::lXXx_or_lXXux<u16, true>(UGeckoInstruction);   // lhzux
// template void CachedInterpreter::lXXx_or_lXXux<s16, false>(UGeckoInstruction);  // lhax
// template void CachedInterpreter::lXXx_or_lXXux<s16, true>(UGeckoInstruction);   // lhaux

template void CachedInterpreter::stX_or_stXu<u32, false>(UGeckoInstruction);  // stw
template void CachedInterpreter::stX_or_stXu<u32, true>(UGeckoInstruction);   // stwu
template void CachedInterpreter::stX_or_stXu<u8, false>(UGeckoInstruction);   // stb
template void CachedInterpreter::stX_or_stXu<u8, true>(UGeckoInstruction);    // stbu
template void CachedInterpreter::stX_or_stXu<u16, false>(UGeckoInstruction);  // sth
template void CachedInterpreter::stX_or_stXu<u16, true>(UGeckoInstruction);   // sthu

template void CachedInterpreter::stXx_or_stXux<u32, false>(UGeckoInstruction);  // stwx
template void CachedInterpreter::stXx_or_stXux<u32, true>(UGeckoInstruction);   // stwux
template void CachedInterpreter::stXx_or_stXux<u8, false>(UGeckoInstruction);   // stbx
template void CachedInterpreter::stXx_or_stXux<u8, true>(UGeckoInstruction);    // stbux
template void CachedInterpreter::stXx_or_stXux<u16, false>(UGeckoInstruction);  // sthx
template void CachedInterpreter::stXx_or_stXux<u16, true>(UGeckoInstruction);   // sthux
