#pragma once

#include "gt_datahub/quality.hpp"
#include "gt_datahub/timestamp.hpp"
#include "gt_datahub/value.hpp"

#include <cstdint>
#include <optional>

namespace gt::datahub {

/**
 * Snapshot returned by the public read API for a single variable.
 *
 * `quality` is the effective quality observed by readers, not the internal raw
 * quality stored by the runtime. Stale is therefore evaluated lazily before the
 * value reaches the public API.
 */
struct VariableState {
  Value value;
  Quality quality{Quality::Uncertain};
  std::optional<Timestamp> source_timestamp;
  std::optional<Timestamp> hub_timestamp;
  std::uint64_t version{0};
  bool initialized{false};
};

}  // namespace gt::datahub
