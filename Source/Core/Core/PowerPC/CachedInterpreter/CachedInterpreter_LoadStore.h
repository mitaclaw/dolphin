// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/CommonTypes.h"
#include "Common/Concepts.h"

namespace PowerPC
{
class PowerPCManager;
class MMU;
}  // namespace PowerPC

namespace CachedInstruction
{
struct LoadStoreOperands : CachedInterpreter::CheckHaltOperands
{
  PowerPC::MMU& mmu;
  const u32 reg : 16;
  const u32 rA : 16;
  const s32 SIMM;
};

struct LoadStoreIndexedOperands : CachedInterpreter::CheckHaltOperands
{
  PowerPC::MMU& mmu;
  const u32 reg : 16;
  const u32 rA : 16;
  const u32 rB : 16;
};

template <class T>
concept LoadScalars = Common::SameAsAnyOf<T, u32, u8, u16, s16>;
template <class T>
concept StoreScalars = Common::SameAsAnyOf<T, u32, u8, u16>;

template <LoadScalars T, bool with_update, bool absolute>
s32 LoadAndZero(PowerPC::PowerPCState& ppc_state, const LoadStoreOperands& operands);

template <LoadScalars T, bool with_update, bool absolute>
s32 LoadAndZeroIndexed(PowerPC::PowerPCState& ppc_state, const LoadStoreIndexedOperands& operands);

template <StoreScalars T, bool with_update, bool absolute>
s32 Store(PowerPC::PowerPCState& ppc_state, const LoadStoreOperands& operands);

template <StoreScalars T, bool with_update, bool absolute>
s32 StoreIndexed(PowerPC::PowerPCState& ppc_state, const LoadStoreIndexedOperands& operands);
}  // namespace CachedInstruction
