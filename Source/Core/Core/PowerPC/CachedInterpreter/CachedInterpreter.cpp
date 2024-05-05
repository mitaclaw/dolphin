// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

CachedInterpreter::CachedInterpreter(Core::System& system) : JitBase(system), m_block_cache(*this)
{
}

CachedInterpreter::~CachedInterpreter() = default;

void CachedInterpreter::Init()
{
  RefreshConfig();

  AllocCodeSpace(CODE_SIZE);
  ResetFreeMemoryRanges();

  jo.enableBlocklink = false;

  m_block_cache.Init();

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;
}

void CachedInterpreter::Shutdown()
{
  m_block_cache.Shutdown();
}

void CachedInterpreter::ExecuteOneBlock()
{
  const u8* normal_entry = m_block_cache.Dispatch();
  if (!normal_entry)
  {
    Jit(m_ppc_state.pc);
    return;
  }

  while (true)
  {
    const auto callback = *reinterpret_cast<const AnyCallback*>(normal_entry);
    if (const auto distance = callback(normal_entry += sizeof(callback)))
      normal_entry += distance;
    else
      break;
  }
}

void CachedInterpreter::Run()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& cpu = m_system.GetCPU();

  const CPU::State* state_ptr = cpu.GetStatePtr();
  while (cpu.GetState() == CPU::State::Running)
  {
    // Start new timing slice
    // NOTE: Exceptions may change PC
    core_timing.Advance();

    do
    {
      ExecuteOneBlock();
    } while (m_ppc_state.downcount > 0 && *state_ptr == CPU::State::Running);
  }
}

void CachedInterpreter::SingleStep()
{
  // Enter new timing slice
  m_system.GetCoreTiming().Advance();
  ExecuteOneBlock();
}

s32 CachedInterpreter::EndBlock(const EndBlockOperands& operands)
{
  const auto& [ppc_state, downcount, num_load_stores, num_fp_inst] = operands;
  ppc_state.pc = ppc_state.npc;
  ppc_state.downcount -= downcount;
  PowerPC::UpdatePerformanceMonitor(downcount, num_load_stores, num_fp_inst, ppc_state);
  return 0;
}

s32 CachedInterpreter::Interpret(const InterpretOperands& operands)
{
  const auto& [interpreter, func, inst] = operands;
  func(interpreter, inst);
  return sizeof(operands);
}

s32 CachedInterpreter::HLEFunction(const HLEFunctionOperands& operands)
{
  const auto& [system, current_pc, hook_index] = operands;
  system.GetPPCState().pc = current_pc;
  HLE::Execute(Core::CPUThreadGuard{system}, current_pc, hook_index);
  return sizeof(operands);
}

s32 CachedInterpreter::WritePC(const WritePCOperands& operands)
{
  const auto& [ppc_state, current_pc] = operands;
  ppc_state.pc = current_pc;
  ppc_state.npc = current_pc + 4;
  return sizeof(operands);
}

s32 CachedInterpreter::WriteBrokenBlockNPC(const WritePCOperands& operands)
{
  const auto& [ppc_state, current_pc] = operands;
  ppc_state.npc = current_pc;
  return sizeof(operands);
}

s32 CachedInterpreter::CheckFPU(const ExceptionCheckOperands& operands)
{
  const auto& [power_pc, current_pc, downcount] = operands;
  if (auto& ppc_state = power_pc.GetPPCState(); !ppc_state.msr.FP)
  {
    ppc_state.pc = current_pc;
    ppc_state.downcount -= downcount;
    ppc_state.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
    power_pc.CheckExceptions();
    return 0;
  }
  return sizeof(operands);
}

s32 CachedInterpreter::CheckDSI(const ExceptionCheckOperands& operands)
{
  const auto& [power_pc, current_pc, downcount] = operands;
  if (auto& ppc_state = power_pc.GetPPCState(); (ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    ppc_state.pc = current_pc;
    ppc_state.downcount -= downcount;
    power_pc.CheckExceptions();
    return 0;
  }
  return sizeof(operands);
}

s32 CachedInterpreter::CheckProgramException(const ExceptionCheckOperands& operands)
{
  const auto& [power_pc, current_pc, downcount] = operands;
  if (auto& ppc_state = power_pc.GetPPCState(); (ppc_state.Exceptions & EXCEPTION_PROGRAM) != 0)
  {
    ppc_state.pc = current_pc;
    ppc_state.downcount -= downcount;
    power_pc.CheckExceptions();
    return 0;
  }
  return sizeof(operands);
}

s32 CachedInterpreter::CheckBreakpoint(const CheckBreakpointOperands& operands)
{
  const auto& [power_pc, cpu_state, current_pc, downcount] = operands;
  // Calling PowerPCManager::GetPPCState twice produces better assembly.
  power_pc.GetPPCState().pc = current_pc;
  if (power_pc.CheckBreakPoints(); *cpu_state != CPU::State::Running)
  {
    power_pc.GetPPCState().downcount -= downcount;
    return 0;
  }
  return sizeof(operands);
}

s32 CachedInterpreter::CheckIdle(const CheckIdleOperands& operands)
{
  const auto& [system, idle_pc] = operands;
  if (system.GetPPCState().npc == idle_pc)
    system.GetCoreTiming().Idle();
  return sizeof(operands);
}

bool CachedInterpreter::HandleFunctionHooking(u32 address)
{
  // CachedInterpreter inherits from JitBase and is considered a JIT by relevant code.
  // (see JitInterface and how m_mode is set within PowerPC.cpp)
  const auto result = HLE::TryReplaceFunction(m_ppc_symbol_db, address, PowerPC::CoreMode::JIT);
  if (!result)
    return false;

  Write(HLEFunction, {m_system, address, result.hook_index});

  if (result.type != HLE::HookType::Replace)
    return false;

  js.downcountAmount += js.st.numCycles;
  Write(EndBlock, {m_ppc_state, js.downcountAmount, js.numLoadStoreInst, js.numFloatingPointInst});
  return true;
}

bool CachedInterpreter::SetEmitterStateToFreeCodeRegion()
{
  const auto free = m_free_ranges.by_size_begin();
  if (free == m_free_ranges.by_size_end())
  {
    WARN_LOG_FMT(DYNA_REC, "Failed to find free memory region in code region.");
    return false;
  }
  SetCodePtr(free.from(), free.to());
  return true;
}

void CachedInterpreter::FreeRanges()
{
  for (const auto& [from, to] : m_block_cache.GetRangesToFree())
    m_free_ranges.insert(from, to);
  m_block_cache.ClearRangesToFree();
}

void CachedInterpreter::ResetFreeMemoryRanges()
{
  m_free_ranges.clear();
  m_free_ranges.insert(region, region + region_size);
}

void CachedInterpreter::Jit(u32 em_address)
{
  Jit(em_address, true);
}

void CachedInterpreter::Jit(u32 em_address, bool clear_cache_and_retry_on_failure)
{
  if (IsAlmostFull() || SConfig::GetInstance().bJITNoBlockCache)
  {
    ClearCache();
  }
  FreeRanges();

  const u32 nextPC =
      analyzer.Analyze(em_address, &code_block, &m_code_buffer, m_code_buffer.size());
  if (code_block.m_memory_exception)
  {
    // Address of instruction could not be translated
    m_ppc_state.npc = nextPC;
    m_ppc_state.Exceptions |= EXCEPTION_ISI;
    m_system.GetPowerPC().CheckExceptions();
    WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", nextPC);
    return;
  }

  if (SetEmitterStateToFreeCodeRegion())
  {
    JitBlock* b = m_block_cache.AllocateBlock(em_address);
    b->normalEntry = b->near_begin = GetWritableCodePtr();

    if (DoJit(em_address, b, nextPC))
    {
      // Record what memory region was used so we know what to free if this block gets invalidated.
      b->near_end = GetWritableCodePtr();
      b->far_begin = b->far_end = nullptr;

      b->codeSize = static_cast<u32>(GetCodePtr() - b->normalEntry);
      b->originalSize = code_block.m_num_instructions;

      // Mark the memory region that this code block uses in the RangeSizeSet.
      if (b->near_begin != b->near_end)
        m_free_ranges.erase(b->near_begin, b->near_end);

      m_block_cache.FinalizeBlock(*b, jo.enableBlocklink, code_block.m_physical_addresses);

      return;
    }
  }

  if (clear_cache_and_retry_on_failure)
  {
    WARN_LOG_FMT(DYNA_REC, "flushing code caches, please report if this happens a lot");
    ClearCache();
    Jit(em_address, false);
    return;
  }

  PanicAlertFmtT("JIT failed to find code space after a cache clear. This should never happen. "
                 "Please report this incident on the bug tracker. Dolphin will now exit.");
  std::exit(-1);
}

bool CachedInterpreter::DoJit(u32 em_address, JitBlock* b, u32 nextPC)
{
  js.blockStart = em_address;
  js.firstFPInstructionFound = false;
  js.fifoBytesSinceCheck = 0;
  js.downcountAmount = 0;
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;
  js.curBlock = b;

  auto& interpreter = m_system.GetInterpreter();
  auto& power_pc = m_system.GetPowerPC();
  auto& breakpoints = power_pc.GetBreakPoints();

  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    PPCAnalyst::CodeOp& op = m_code_buffer[i];
    js.op = &op;

    js.compilerPC = op.address;
    js.instructionsLeft = (code_block.m_num_instructions - 1) - i;
    js.downcountAmount += op.opinfo->num_cycles;
    if (op.opinfo->flags & FL_LOADSTORE)
      ++js.numLoadStoreInst;
    if (op.opinfo->flags & FL_USE_FPU)
      ++js.numFloatingPointInst;

    if (HandleFunctionHooking(js.compilerPC))
      break;

    if (!op.skip)
    {
      const bool breakpoint =
          IsDebuggingEnabled() && breakpoints.IsAddressBreakPoint(js.compilerPC);
      const bool check_fpu = (op.opinfo->flags & FL_USE_FPU) != 0 && !js.firstFPInstructionFound;
      const bool endblock = (op.opinfo->flags & FL_ENDBLOCK) != 0;
      const bool memcheck = (op.opinfo->flags & FL_LOADSTORE) != 0 && jo.memcheck;
      const bool check_program_exception = !endblock && ShouldHandleFPExceptionForInstruction(&op);
      const bool idle_loop = op.branchIsIdleLoop;

      if (breakpoint)
      {
        Write(CheckBreakpoint,
              {power_pc, m_system.GetCPU().GetStatePtr(), js.compilerPC, js.downcountAmount});
      }
      if (check_fpu)
      {
        Write(CheckFPU, {power_pc, js.compilerPC, js.downcountAmount});
        js.firstFPInstructionFound = true;
      }

      if (endblock)
        Write(WritePC, {m_ppc_state, js.compilerPC});
      Write(Interpret, {interpreter, Interpreter::GetInterpreterOp(op.inst), op.inst});
      if (memcheck)
        Write(CheckDSI, {power_pc, js.compilerPC, js.downcountAmount});
      if (check_program_exception)
        Write(CheckProgramException, {power_pc, js.compilerPC, js.downcountAmount});
      if (idle_loop)
        Write(CheckIdle, {m_system, js.blockStart});
      if (endblock)
      {
        Write(EndBlock,
              {m_ppc_state, js.downcountAmount, js.numLoadStoreInst, js.numFloatingPointInst});
      }
    }
  }
  if (code_block.m_broken)
  {
    Write(WriteBrokenBlockNPC, {m_ppc_state, nextPC});
    Write(EndBlock,
          {m_ppc_state, js.downcountAmount, js.numLoadStoreInst, js.numFloatingPointInst});
  }

  if (HasWriteFailed())
  {
    WARN_LOG_FMT(DYNA_REC, "JIT ran out of space in code region during code generation.");
    return false;
  }
  return true;
}

void CachedInterpreter::ClearCache()
{
  m_block_cache.Clear();
  m_block_cache.ClearRangesToFree();
  ClearCodeSpace();
  ResetFreeMemoryRanges();
  RefreshConfig();
}
