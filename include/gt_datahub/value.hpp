#pragma once

#include "gt_datahub/timestamp.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace gt::datahub {

/**
 * Canonical value container used by the public DataHub contract.
 *
 * The variant alternatives mirror the baseline exactly, including
 * `std::monostate` for "no value" and `Timestamp` for date-time payloads.
 */
using Value = std::variant<
    std::monostate,
    bool,
    std::int32_t,
    std::uint32_t,
    std::int64_t,
    std::uint64_t,
    float,
    double,
    std::string,
    Timestamp>;

}  // namespace gt::datahub
