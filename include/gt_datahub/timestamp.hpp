#pragma once

#include <chrono>

namespace gt::datahub {

/**
 * Public wall-clock timestamp exposed by the DataHub API.
 *
 * This alias represents when an event happened in the real world and is the
 * type used for source and hub timestamps. Runtime interval calculations such
 * as stale, scheduler periods and timeouts must use `std::chrono::steady_clock`
 * internally instead.
 */
using Timestamp = std::chrono::system_clock::time_point;

}  // namespace gt::datahub
