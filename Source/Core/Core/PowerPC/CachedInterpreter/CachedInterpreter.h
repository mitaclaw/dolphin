// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>

#include <rangeset/rangesizeset.h>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreterBlockCache.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreterEmitter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PPCAnalyst.h"

class Interpreter;
namespace CPU
{
enum class State;
}

class CachedInterpreter : public JitBase, public CachedInterpreterCodeBlock
{
public:
  explicit CachedInterpreter(Core::System& system);
  CachedInterpreter(const CachedInterpreter&) = delete;
  CachedInterpreter(CachedInterpreter&&) = delete;
  CachedInterpreter& operator=(const CachedInterpreter&) = delete;
  CachedInterpreter& operator=(CachedInterpreter&&) = delete;
  ~CachedInterpreter();

  void Init() override;
  void Shutdown() override;

  bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }
  void ClearCache() override;

  void Run() override;
  void SingleStep() override;

  void Jit(u32 address) override;
  void Jit(u32 address, bool clear_cache_and_retry_on_failure);
  bool DoJit(u32 address, JitBlock* b, u32 nextPC);

  JitBaseBlockCache* GetBlockCache() override { return &m_block_cache; }
  const char* GetName() const override { return "Cached Interpreter"; }
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }

private:
  void ExecuteOneBlock();

  bool HandleFunctionHooking(u32 address);

  // Finds a free memory region and sets the code emitter to point at that region.
  // Returns false if no free memory region can be found.
  bool SetEmitterStateToFreeCodeRegion();

  void FreeRanges();
  void ResetFreeMemoryRanges();

  struct EndBlockOperands;
  struct InterpretOperands;
  struct HLEFunctionOperands;
  struct WritePCOperands;
  struct ExceptionCheckOperands;
  struct CheckBreakpointOperands;
  struct CheckIdleOperands;

  // The return value of most callback is sizeof(operands).
  // If the return value is 0, the block is finished.
  static s32 EndBlock(const EndBlockOperands& operands);
  static s32 Interpret(const InterpretOperands& operands);
  static s32 HLEFunction(const HLEFunctionOperands& operands);
  static s32 WritePC(const WritePCOperands& operands);
  static s32 WriteBrokenBlockNPC(const WritePCOperands& operands);
  static s32 CheckFPU(const ExceptionCheckOperands& operands);
  static s32 CheckDSI(const ExceptionCheckOperands& operands);
  static s32 CheckProgramException(const ExceptionCheckOperands& operands);
  static s32 CheckBreakpoint(const CheckBreakpointOperands& operands);
  static s32 CheckIdle(const CheckIdleOperands& operands);

  HyoutaUtilities::RangeSizeSet<u8*> m_free_ranges;
  CachedInterpreterBlockCache m_block_cache;
};

struct CachedInterpreter::EndBlockOperands
{
  PowerPC::PowerPCState& ppc_state;
  u32 downcount;
  u32 num_load_stores;
  u32 num_fp_inst;
};

struct CachedInterpreter::InterpretOperands
{
  Interpreter& interpreter;
  void (*func)(Interpreter&, UGeckoInstruction);  // Interpreter::Instruction
  UGeckoInstruction inst;
};

struct CachedInterpreter::HLEFunctionOperands
{
  Core::System& system;
  u32 current_pc;
  u32 hook_index;
};

struct CachedInterpreter::WritePCOperands
{
  PowerPC::PowerPCState& ppc_state;
  u32 current_pc;
};

struct CachedInterpreter::ExceptionCheckOperands
{
  PowerPC::PowerPCManager& power_pc;
  u32 downcount;
};

struct CachedInterpreter::CheckBreakpointOperands
{
  PowerPC::PowerPCManager& power_pc;
  const CPU::State* cpu_state;
  u32 downcount;
};

struct CachedInterpreter::CheckIdleOperands
{
  Core::System& system;
  u32 idle_pc;
};
