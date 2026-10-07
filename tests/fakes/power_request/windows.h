#pragma once
#ifndef IDLEHARBOR_POWER_REQUEST_TEST_DOUBLE
#error This header belongs only to the isolated power-request test target.
#endif
#include <cstdint>
using EXECUTION_STATE = std::uint32_t;
inline constexpr EXECUTION_STATE ES_CONTINUOUS = 0x80000000u;
inline constexpr EXECUTION_STATE ES_SYSTEM_REQUIRED = 0x00000001u;
inline constexpr EXECUTION_STATE ES_DISPLAY_REQUIRED = 0x00000002u;
EXECUTION_STATE SetThreadExecutionState(EXECUTION_STATE flags);
