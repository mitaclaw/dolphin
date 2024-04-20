// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include <array>

#include "Common/Assert.h"
#include "Common/TypeUtils.h"
#include "Core/PowerPC/Gekko.h"

namespace
{
struct CachedInterpreterOpTemplate
{
  u32 opcode;
  CachedInterpreter::Instruction fn;
};

constexpr std::array<CachedInterpreterOpTemplate, 54> s_primary_table{{
    {4, &CachedInterpreter::DynaRunTable4},    // RunTable4
    {19, &CachedInterpreter::DynaRunTable19},  // RunTable19
    {31, &CachedInterpreter::DynaRunTable31},  // RunTable31
    {59, &CachedInterpreter::DynaRunTable59},  // RunTable59
    {63, &CachedInterpreter::DynaRunTable63},  // RunTable63

    {16, &CachedInterpreter::FallBackToInterpreter},  // bcx
    {18, &CachedInterpreter::bx},                     // bx

    {3, &CachedInterpreter::FallBackToInterpreter},   // twi
    {17, &CachedInterpreter::FallBackToInterpreter},  // sc

    {7, &CachedInterpreter::FallBackToInterpreter},   // mulli
    {8, &CachedInterpreter::FallBackToInterpreter},   // subfic
    {10, &CachedInterpreter::FallBackToInterpreter},  // cmpli
    {11, &CachedInterpreter::FallBackToInterpreter},  // cmpi
    {12, &CachedInterpreter::FallBackToInterpreter},  // addic
    {13, &CachedInterpreter::FallBackToInterpreter},  // addic_rc
    {14, &CachedInterpreter::addi},                   // addi
    {15, &CachedInterpreter::addis},                  // addis

    {20, &CachedInterpreter::FallBackToInterpreter},  // rlwimix
    {21, &CachedInterpreter::FallBackToInterpreter},  // rlwinmx
    {23, &CachedInterpreter::FallBackToInterpreter},  // rlwnmx

    {24, &CachedInterpreter::FallBackToInterpreter},  // ori
    {25, &CachedInterpreter::FallBackToInterpreter},  // oris
    {26, &CachedInterpreter::FallBackToInterpreter},  // xori
    {27, &CachedInterpreter::FallBackToInterpreter},  // xoris
    {28, &CachedInterpreter::FallBackToInterpreter},  // andi_rc
    {29, &CachedInterpreter::FallBackToInterpreter},  // andis_rc

    {32, &CachedInterpreter::lXX_or_lXXu<u32, false>},  // lwz
    {33, &CachedInterpreter::lXX_or_lXXu<u32, true>},   // lwzu
    {34, &CachedInterpreter::lXX_or_lXXu<u8, false>},   // lbz
    {35, &CachedInterpreter::lXX_or_lXXu<u8, true>},    // lbzu
    {40, &CachedInterpreter::lXX_or_lXXu<u16, false>},  // lhz
    {41, &CachedInterpreter::lXX_or_lXXu<u16, true>},   // lhzu
    {42, &CachedInterpreter::FallBackToInterpreter},    // lha
    {43, &CachedInterpreter::FallBackToInterpreter},    // lhau

    {36, &CachedInterpreter::stX_or_stXu<u32, false>},  // stw
    {37, &CachedInterpreter::stX_or_stXu<u32, true>},   // stwu
    {38, &CachedInterpreter::stX_or_stXu<u8, false>},   // stb
    {39, &CachedInterpreter::stX_or_stXu<u8, true>},    // stbu
    {44, &CachedInterpreter::stX_or_stXu<u16, false>},  // sth
    {45, &CachedInterpreter::stX_or_stXu<u16, true>},   // sthu

    {46, &CachedInterpreter::FallBackToInterpreter},  // lmw
    {47, &CachedInterpreter::FallBackToInterpreter},  // stmw

    {48, &CachedInterpreter::FallBackToInterpreter},  // lfs
    {49, &CachedInterpreter::FallBackToInterpreter},  // lfsu
    {50, &CachedInterpreter::FallBackToInterpreter},  // lfd
    {51, &CachedInterpreter::FallBackToInterpreter},  // lfdu

    {52, &CachedInterpreter::FallBackToInterpreter},  // stfs
    {53, &CachedInterpreter::FallBackToInterpreter},  // stfsu
    {54, &CachedInterpreter::FallBackToInterpreter},  // stfd
    {55, &CachedInterpreter::FallBackToInterpreter},  // stfdu

    {56, &CachedInterpreter::FallBackToInterpreter},  // psq_l
    {57, &CachedInterpreter::FallBackToInterpreter},  // psq_lu
    {60, &CachedInterpreter::FallBackToInterpreter},  // psq_st
    {61, &CachedInterpreter::FallBackToInterpreter},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

constexpr std::array<CachedInterpreterOpTemplate, 13> s_table4{{
    // SUBOP10
    {0, &CachedInterpreter::FallBackToInterpreter},    // ps_cmpu0
    {32, &CachedInterpreter::FallBackToInterpreter},   // ps_cmpo0
    {40, &CachedInterpreter::FallBackToInterpreter},   // ps_neg
    {136, &CachedInterpreter::FallBackToInterpreter},  // ps_nabs
    {264, &CachedInterpreter::FallBackToInterpreter},  // ps_abs
    {64, &CachedInterpreter::FallBackToInterpreter},   // ps_cmpu1
    {72, &CachedInterpreter::FallBackToInterpreter},   // ps_mr
    {96, &CachedInterpreter::FallBackToInterpreter},   // ps_cmpo1
    {528, &CachedInterpreter::FallBackToInterpreter},  // ps_merge00
    {560, &CachedInterpreter::FallBackToInterpreter},  // ps_merge01
    {592, &CachedInterpreter::FallBackToInterpreter},  // ps_merge10
    {624, &CachedInterpreter::FallBackToInterpreter},  // ps_merge11

    {1014, &CachedInterpreter::FallBackToInterpreter},  // dcbz_l
}};

constexpr std::array<CachedInterpreterOpTemplate, 17> s_table4_2{{
    {10, &CachedInterpreter::FallBackToInterpreter},  // ps_sum0
    {11, &CachedInterpreter::FallBackToInterpreter},  // ps_sum1
    {12, &CachedInterpreter::FallBackToInterpreter},  // ps_muls0
    {13, &CachedInterpreter::FallBackToInterpreter},  // ps_muls1
    {14, &CachedInterpreter::FallBackToInterpreter},  // ps_madds0
    {15, &CachedInterpreter::FallBackToInterpreter},  // ps_madds1
    {18, &CachedInterpreter::FallBackToInterpreter},  // ps_div
    {20, &CachedInterpreter::FallBackToInterpreter},  // ps_sub
    {21, &CachedInterpreter::FallBackToInterpreter},  // ps_add
    {23, &CachedInterpreter::FallBackToInterpreter},  // ps_sel
    {24, &CachedInterpreter::FallBackToInterpreter},  // ps_res
    {25, &CachedInterpreter::FallBackToInterpreter},  // ps_mul
    {26, &CachedInterpreter::FallBackToInterpreter},  // ps_rsqrte
    {28, &CachedInterpreter::FallBackToInterpreter},  // ps_msub
    {29, &CachedInterpreter::FallBackToInterpreter},  // ps_madd
    {30, &CachedInterpreter::FallBackToInterpreter},  // ps_nmsub
    {31, &CachedInterpreter::FallBackToInterpreter},  // ps_nmadd
}};

constexpr std::array<CachedInterpreterOpTemplate, 4> s_table4_3{{
    {6, &CachedInterpreter::FallBackToInterpreter},   // psq_lx
    {7, &CachedInterpreter::FallBackToInterpreter},   // psq_stx
    {38, &CachedInterpreter::FallBackToInterpreter},  // psq_lux
    {39, &CachedInterpreter::FallBackToInterpreter},  // psq_stux
}};

constexpr std::array<CachedInterpreterOpTemplate, 13> s_table19{{
    {528, &CachedInterpreter::bcctrx},                 // bcctrx
    {16, &CachedInterpreter::bclrx},                   // bclrx
    {257, &CachedInterpreter::FallBackToInterpreter},  // crand
    {129, &CachedInterpreter::FallBackToInterpreter},  // crandc
    {289, &CachedInterpreter::FallBackToInterpreter},  // creqv
    {225, &CachedInterpreter::FallBackToInterpreter},  // crnand
    {33, &CachedInterpreter::FallBackToInterpreter},   // crnor
    {449, &CachedInterpreter::FallBackToInterpreter},  // cror
    {417, &CachedInterpreter::FallBackToInterpreter},  // crorc
    {193, &CachedInterpreter::FallBackToInterpreter},  // crxor

    {150, &CachedInterpreter::FallBackToInterpreter},  // isync
    {0, &CachedInterpreter::FallBackToInterpreter},    // mcrf

    {50, &CachedInterpreter::FallBackToInterpreter},  // rfi
}};

constexpr std::array<CachedInterpreterOpTemplate, 107> s_table31{{
    {266, &CachedInterpreter::FallBackToInterpreter},   // addx
    {778, &CachedInterpreter::FallBackToInterpreter},   // addox
    {10, &CachedInterpreter::FallBackToInterpreter},    // addcx
    {522, &CachedInterpreter::FallBackToInterpreter},   // addcox
    {138, &CachedInterpreter::FallBackToInterpreter},   // addex
    {650, &CachedInterpreter::FallBackToInterpreter},   // addeox
    {234, &CachedInterpreter::FallBackToInterpreter},   // addmex
    {746, &CachedInterpreter::FallBackToInterpreter},   // addmeox
    {202, &CachedInterpreter::FallBackToInterpreter},   // addzex
    {714, &CachedInterpreter::FallBackToInterpreter},   // addzeox
    {491, &CachedInterpreter::FallBackToInterpreter},   // divwx
    {1003, &CachedInterpreter::FallBackToInterpreter},  // divwox
    {459, &CachedInterpreter::FallBackToInterpreter},   // divwux
    {971, &CachedInterpreter::FallBackToInterpreter},   // divwuox
    {75, &CachedInterpreter::FallBackToInterpreter},    // mulhwx
    {11, &CachedInterpreter::FallBackToInterpreter},    // mulhwux
    {235, &CachedInterpreter::FallBackToInterpreter},   // mullwx
    {747, &CachedInterpreter::FallBackToInterpreter},   // mullwox
    {104, &CachedInterpreter::FallBackToInterpreter},   // negx
    {616, &CachedInterpreter::FallBackToInterpreter},   // negox
    {40, &CachedInterpreter::FallBackToInterpreter},    // subfx
    {552, &CachedInterpreter::FallBackToInterpreter},   // subfox
    {8, &CachedInterpreter::FallBackToInterpreter},     // subfcx
    {520, &CachedInterpreter::FallBackToInterpreter},   // subfcox
    {136, &CachedInterpreter::FallBackToInterpreter},   // subfex
    {648, &CachedInterpreter::FallBackToInterpreter},   // subfeox
    {232, &CachedInterpreter::FallBackToInterpreter},   // subfmex
    {744, &CachedInterpreter::FallBackToInterpreter},   // subfmeox
    {200, &CachedInterpreter::FallBackToInterpreter},   // subfzex
    {712, &CachedInterpreter::FallBackToInterpreter},   // subfzeox

    {28, &CachedInterpreter::FallBackToInterpreter},   // andx
    {60, &CachedInterpreter::FallBackToInterpreter},   // andcx
    {444, &CachedInterpreter::FallBackToInterpreter},  // orx
    {124, &CachedInterpreter::FallBackToInterpreter},  // norx
    {316, &CachedInterpreter::FallBackToInterpreter},  // xorx
    {412, &CachedInterpreter::FallBackToInterpreter},  // orcx
    {476, &CachedInterpreter::FallBackToInterpreter},  // nandx
    {284, &CachedInterpreter::FallBackToInterpreter},  // eqvx
    {0, &CachedInterpreter::FallBackToInterpreter},    // cmp
    {32, &CachedInterpreter::FallBackToInterpreter},   // cmpl
    {26, &CachedInterpreter::FallBackToInterpreter},   // cntlzwx
    {922, &CachedInterpreter::FallBackToInterpreter},  // extshx
    {954, &CachedInterpreter::FallBackToInterpreter},  // extsbx
    {536, &CachedInterpreter::FallBackToInterpreter},  // srwx
    {792, &CachedInterpreter::FallBackToInterpreter},  // srawx
    {824, &CachedInterpreter::FallBackToInterpreter},  // srawix
    {24, &CachedInterpreter::FallBackToInterpreter},   // slwx

    {54, &CachedInterpreter::FallBackToInterpreter},    // dcbst
    {86, &CachedInterpreter::FallBackToInterpreter},    // dcbf
    {246, &CachedInterpreter::FallBackToInterpreter},   // dcbtst
    {278, &CachedInterpreter::FallBackToInterpreter},   // dcbt
    {470, &CachedInterpreter::FallBackToInterpreter},   // dcbi
    {758, &CachedInterpreter::FallBackToInterpreter},   // dcba
    {1014, &CachedInterpreter::FallBackToInterpreter},  // dcbz

    // load word
    {23, &CachedInterpreter::lXXx_or_lXXux<u32, false>},  // lwzx
    {55, &CachedInterpreter::lXXx_or_lXXux<u32, true>},   // lwzux

    // load halfword
    {279, &CachedInterpreter::lXXx_or_lXXux<u16, false>},  // lhzx
    {311, &CachedInterpreter::lXXx_or_lXXux<u16, true>},   // lhzux

    // load halfword signextend
    {343, &CachedInterpreter::FallBackToInterpreter},  // lhax
    {375, &CachedInterpreter::FallBackToInterpreter},  // lhaux

    // load byte
    {87, &CachedInterpreter::lXXx_or_lXXux<u8, false>},  // lbzx
    {119, &CachedInterpreter::lXXx_or_lXXux<u8, true>},  // lbzux

    // load byte reverse
    {534, &CachedInterpreter::FallBackToInterpreter},  // lwbrx
    {790, &CachedInterpreter::FallBackToInterpreter},  // lhbrx

    // Conditional load/store (Wii SMP)
    {150, &CachedInterpreter::FallBackToInterpreter},  // stwcxd
    {20, &CachedInterpreter::FallBackToInterpreter},   // lwarx

    // load string (interpret these)
    {533, &CachedInterpreter::FallBackToInterpreter},  // lswx
    {597, &CachedInterpreter::FallBackToInterpreter},  // lswi

    // store word
    {151, &CachedInterpreter::stXx_or_stXux<u32, false>},  // stwx
    {183, &CachedInterpreter::stXx_or_stXux<u32, true>},   // stwux

    // store byte
    {215, &CachedInterpreter::stXx_or_stXux<u8, false>},  // stbx
    {247, &CachedInterpreter::stXx_or_stXux<u8, true>},   // stbux

    // store halfword
    {407, &CachedInterpreter::stXx_or_stXux<u16, false>},  // sthx
    {439, &CachedInterpreter::stXx_or_stXux<u16, true>},   // sthux

    // store bytereverse
    {662, &CachedInterpreter::FallBackToInterpreter},  // stwbrx
    {918, &CachedInterpreter::FallBackToInterpreter},  // sthbrx

    {661, &CachedInterpreter::FallBackToInterpreter},  // stswx
    {725, &CachedInterpreter::FallBackToInterpreter},  // stswi

    // fp load/store
    {535, &CachedInterpreter::FallBackToInterpreter},  // lfsx
    {567, &CachedInterpreter::FallBackToInterpreter},  // lfsux
    {599, &CachedInterpreter::FallBackToInterpreter},  // lfdx
    {631, &CachedInterpreter::FallBackToInterpreter},  // lfdux

    {663, &CachedInterpreter::FallBackToInterpreter},  // stfsx
    {695, &CachedInterpreter::FallBackToInterpreter},  // stfsux
    {727, &CachedInterpreter::FallBackToInterpreter},  // stfdx
    {759, &CachedInterpreter::FallBackToInterpreter},  // stfdux
    {983, &CachedInterpreter::FallBackToInterpreter},  // stfiwx

    {19, &CachedInterpreter::FallBackToInterpreter},   // mfcr
    {83, &CachedInterpreter::FallBackToInterpreter},   // mfmsr
    {144, &CachedInterpreter::FallBackToInterpreter},  // mtcrf
    {146, &CachedInterpreter::FallBackToInterpreter},  // mtmsr
    {210, &CachedInterpreter::FallBackToInterpreter},  // mtsr
    {242, &CachedInterpreter::FallBackToInterpreter},  // mtsrin
    {339, &CachedInterpreter::FallBackToInterpreter},  // mfspr
    {467, &CachedInterpreter::FallBackToInterpreter},  // mtspr
    {371, &CachedInterpreter::FallBackToInterpreter},  // mftb
    {512, &CachedInterpreter::FallBackToInterpreter},  // mcrxr
    {595, &CachedInterpreter::FallBackToInterpreter},  // mfsr
    {659, &CachedInterpreter::FallBackToInterpreter},  // mfsrin

    {4, &CachedInterpreter::FallBackToInterpreter},    // tw
    {598, &CachedInterpreter::FallBackToInterpreter},  // sync
    {982, &CachedInterpreter::FallBackToInterpreter},  // icbi

    // Unused instructions on GC
    {310, &CachedInterpreter::FallBackToInterpreter},  // eciwx
    {438, &CachedInterpreter::FallBackToInterpreter},  // ecowx
    {854, &CachedInterpreter::FallBackToInterpreter},  // eieio
    {306, &CachedInterpreter::FallBackToInterpreter},  // tlbie
    {566, &CachedInterpreter::FallBackToInterpreter},  // tlbsync
}};

constexpr std::array<CachedInterpreterOpTemplate, 9> s_table59{{
    {18, &CachedInterpreter::FallBackToInterpreter},  // fdivsx
    {20, &CachedInterpreter::FallBackToInterpreter},  // fsubsx
    {21, &CachedInterpreter::FallBackToInterpreter},  // faddsx
    {24, &CachedInterpreter::FallBackToInterpreter},  // fresx
    {25, &CachedInterpreter::FallBackToInterpreter},  // fmulsx
    {28, &CachedInterpreter::FallBackToInterpreter},  // fmsubsx
    {29, &CachedInterpreter::FallBackToInterpreter},  // fmaddsx
    {30, &CachedInterpreter::FallBackToInterpreter},  // fnmsubsx
    {31, &CachedInterpreter::FallBackToInterpreter},  // fnmaddsx
}};

constexpr std::array<CachedInterpreterOpTemplate, 15> s_table63{{
    {264, &CachedInterpreter::FallBackToInterpreter},  // fabsx
    {32, &CachedInterpreter::FallBackToInterpreter},   // fcmpo
    {0, &CachedInterpreter::FallBackToInterpreter},    // fcmpu
    {14, &CachedInterpreter::FallBackToInterpreter},   // fctiwx
    {15, &CachedInterpreter::FallBackToInterpreter},   // fctiwzx
    {72, &CachedInterpreter::FallBackToInterpreter},   // fmrx
    {136, &CachedInterpreter::FallBackToInterpreter},  // fnabsx
    {40, &CachedInterpreter::FallBackToInterpreter},   // fnegx
    {12, &CachedInterpreter::FallBackToInterpreter},   // frspx

    {64, &CachedInterpreter::FallBackToInterpreter},   // mcrfs
    {583, &CachedInterpreter::FallBackToInterpreter},  // mffsx
    {70, &CachedInterpreter::FallBackToInterpreter},   // mtfsb0x
    {38, &CachedInterpreter::FallBackToInterpreter},   // mtfsb1x
    {134, &CachedInterpreter::FallBackToInterpreter},  // mtfsfix
    {711, &CachedInterpreter::FallBackToInterpreter},  // mtfsfx
}};

constexpr std::array<CachedInterpreterOpTemplate, 10> s_table63_2{{
    {18, &CachedInterpreter::FallBackToInterpreter},  // fdivx
    {20, &CachedInterpreter::FallBackToInterpreter},  // fsubx
    {21, &CachedInterpreter::FallBackToInterpreter},  // faddx
    {23, &CachedInterpreter::FallBackToInterpreter},  // fselx
    {25, &CachedInterpreter::FallBackToInterpreter},  // fmulx
    {26, &CachedInterpreter::FallBackToInterpreter},  // frsqrtex
    {28, &CachedInterpreter::FallBackToInterpreter},  // fmsubx
    {29, &CachedInterpreter::FallBackToInterpreter},  // fmaddx
    {30, &CachedInterpreter::FallBackToInterpreter},  // fnmsubx
    {31, &CachedInterpreter::FallBackToInterpreter},  // fnmaddx
}};

constexpr std::array<CachedInterpreter::Instruction, 64> s_dyna_op_table = []() consteval {
  std::array<CachedInterpreter::Instruction, 64> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (auto& tpl : s_primary_table)
  {
    ASSERT(table[tpl.opcode] == &CachedInterpreter::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<CachedInterpreter::Instruction, 1024> s_dyna_op_table4 = []() consteval {
  std::array<CachedInterpreter::Instruction, 1024> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table4_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &CachedInterpreter::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (u32 i = 0; i < 16; i++)
  {
    const u32 fill = i << 6;
    for (const auto& tpl : s_table4_3)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &CachedInterpreter::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(table[op] == &CachedInterpreter::FallBackToInterpreter);
    table[op] = tpl.fn;
  }

  return table;
}();

constexpr std::array<CachedInterpreter::Instruction, 1024> s_dyna_op_table19 = []() consteval {
  std::array<CachedInterpreter::Instruction, 1024> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (const auto& tpl : s_table19)
  {
    ASSERT(table[tpl.opcode] == &CachedInterpreter::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<CachedInterpreter::Instruction, 1024> s_dyna_op_table31 = []() consteval {
  std::array<CachedInterpreter::Instruction, 1024> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (const auto& tpl : s_table31)
  {
    ASSERT(table[tpl.opcode] == &CachedInterpreter::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<CachedInterpreter::Instruction, 32> s_dyna_op_table59 = []() consteval {
  std::array<CachedInterpreter::Instruction, 32> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (const auto& tpl : s_table59)
  {
    ASSERT(table[tpl.opcode] == &CachedInterpreter::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<CachedInterpreter::Instruction, 1024> s_dyna_op_table63 = []() consteval {
  std::array<CachedInterpreter::Instruction, 1024> table{};
  table.fill(&CachedInterpreter::FallBackToInterpreter);

  for (const auto& tpl : s_table63)
  {
    ASSERT(table[tpl.opcode] == &CachedInterpreter::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table63_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &CachedInterpreter::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  return table;
}();

}  // Anonymous namespace

void CachedInterpreter::DynaRunTable4(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table4[inst.SUBOP10])(inst);
}

void CachedInterpreter::DynaRunTable19(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table19[inst.SUBOP10])(inst);
}

void CachedInterpreter::DynaRunTable31(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table31[inst.SUBOP10])(inst);
}

void CachedInterpreter::DynaRunTable59(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table59[inst.SUBOP5])(inst);
}

void CachedInterpreter::DynaRunTable63(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table63[inst.SUBOP10])(inst);
}

void CachedInterpreter::CompileInstruction(const PPCAnalyst::CodeOp& op)
{
  (this->*s_dyna_op_table[op.inst.OPCD])(op.inst);
  PPCTables::CountInstructionCompile(op.opinfo, js.compilerPC);
}
