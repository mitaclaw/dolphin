// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <type_traits>

#include "Common/CodeBlock.h"
#include "Common/CommonTypes.h"

class CachedInterpreterEmitter
{
protected:
  // The lone parameter is a type-erased reference to operands.
  // 32-bit return values seem to perform better than 64-bit ones.
  using AnyCallback = s32 (*)(const void* operands);

private:
  void Write(AnyCallback callback, const void* operands, std::size_t size);

public:
  template <class Operands>
  void Write(s32 (*callback)(const Operands&), const Operands& operands)
  {
    // I would use std::is_trivial_v, but almost every operands struct uses
    // references instead of pointers to make the callback functions nicer.
    static_assert(
        std::is_trivially_copyable_v<Operands> && std::is_trivially_destructible_v<Operands> &&
        alignof(Operands) <= alignof(AnyCallback) && sizeof(Operands) % alignof(AnyCallback) == 0);
    Write(reinterpret_cast<AnyCallback>(callback), &operands, sizeof(Operands));
  }

  const u8* GetCodePtr() const { return m_code; }
  u8* GetWritableCodePtr() { return m_code; }
  const u8* GetCodeEnd() const { return m_code_end; };
  u8* GetWritableCodeEnd() { return m_code_end; };
  // Should be checked after a block of code has been generated to see if the code has been
  // successfully written to memory. Do not call the generated code when this returns true!
  bool HasWriteFailed() const { return m_write_failed; }

  void SetCodePtr(u8* begin, u8* end)
  {
    m_code = begin;
    m_code_end = end;
    m_write_failed = false;
  };

private:
  // Pointer to memory where code will be emitted to.
  u8* m_code = nullptr;
  // Pointer past the end of the memory region we're allowed to emit to.
  // Writes that would reach this memory are refused and will set the m_write_failed flag instead.
  u8* m_code_end = nullptr;
  // Set to true when a write request happens that would write past m_code_end.
  // Must be cleared with SetCodePtr() afterwards.
  bool m_write_failed = false;
};

class CachedInterpreterCodeBlock : public Common::CodeBlock<CachedInterpreterEmitter, false>
{
public:
  struct PoisonOperands;  // Don't define this incomplete type; it doesn't exist.
  static s32 PoisonCallback(const PoisonOperands& operands);

private:
  void PoisonMemory() override;
};
