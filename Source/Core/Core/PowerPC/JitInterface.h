// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdio>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Core/MachineContext.h"

class CPUCoreBase;
class PointerWrap;
class JitBase;
struct JitBlock;

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core
namespace PowerPC
{
enum class CPUCore;
}

class JitInterface
{
public:
  explicit JitInterface(Core::System& system);
  JitInterface(const JitInterface&) = delete;
  JitInterface(JitInterface&&) = delete;
  JitInterface& operator=(const JitInterface&) = delete;
  JitInterface& operator=(JitInterface&&) = delete;
  ~JitInterface();

  void DoState(PointerWrap& p);

  CPUCoreBase* InitJitCore(PowerPC::CPUCore core);
  CPUCoreBase* GetCore() const;

  // Debugging
  enum class GetHostCodeError
  {
    NoJitActive,
    NoTranslation,
  };
  struct GetHostCodeResult
  {
    const u8* code;
    u32 code_size;
    u32 entry_address;
  };

  void UpdateMembase();
  void JitBlockLogDump(const Core::CPUThreadGuard& guard, std::FILE* file) const;
  void RunOnBlocks(const Core::CPUThreadGuard& guard, std::function<void(const JitBlock&)> f) const;
  void WipeBlockProfilingData(const Core::CPUThreadGuard& guard);
  std::size_t GetBlockCount() const;

  // Memory Utilities
  bool HandleFault(uintptr_t access_address, SContext* ctx);
  bool HandleStackFault();

  // Clearing CodeCache
  void ClearCache(const Core::CPUThreadGuard& guard);

  // This clear is "safe" in the sense that it's okay to run from
  // inside a JIT'ed block: it clears the instruction cache, but not
  // the JIT'ed code.
  void ClearSafe();

  // DolphinQt's JITWidget needs this. Nothing else (from outside of the Core) should use
  // it, or else JitBlockTableModel will contain a dangling reference. If something else
  // from outside of the Core *must* use this, consider reworking the logic in JITWidget.
  void EraseSingleBlock(const JitBlock& block);
  // Disassemble the recompiled code from a JIT block.
  void DisasmNearCode(const JitBlock& block, std::ostream& stream,
                      std::size_t& instruction_count) const;
  void DisasmFarCode(const JitBlock& block, std::ostream& stream,
                     std::size_t& instruction_count) const;
  // Pair contains free size + fragmentation ratio
  std::pair<std::size_t, double> GetNearMemoryInfo() const;
  std::pair<std::size_t, double> GetFarMemoryInfo() const;

  // If "forced" is true, a recompile is being requested on code that hasn't been modified.
  void InvalidateICache(u32 address, u32 size, bool forced);
  void InvalidateICacheLine(u32 address);
  void InvalidateICacheLines(u32 address, u32 count);
  static void InvalidateICacheLineFromJIT(JitInterface& jit_interface, u32 address);
  static void InvalidateICacheLinesFromJIT(JitInterface& jit_interface, u32 address, u32 count);

  enum class ExceptionType
  {
    FIFOWrite,
    PairedQuantize,
    SpeculativeConstants
  };
  void CompileExceptionCheck(ExceptionType type);
  static void CompileExceptionCheckFromJIT(JitInterface& jit_interface, ExceptionType type);

  /// used for the page fault unit test, don't use outside of tests!
  void SetJit(std::unique_ptr<JitBase> jit);

  void Shutdown();

private:
  std::unique_ptr<JitBase> m_jit;
  Core::System& m_system;
};
