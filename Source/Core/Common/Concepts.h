// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>

namespace Common
{
template <class T, class... Ts>
concept SameAsAnyOf = std::disjunction_v<std::is_same<T, Ts>...>;
}
