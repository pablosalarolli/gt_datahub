#pragma once

#include "gt_datahub/quality.hpp"
#include "gt_datahub/timestamp.hpp"
#include "gt_datahub/value.hpp"

#include <optional>

namespace gt::datahub {

/**
 * Payload accepted by producer-side write operations.
 *
 * The request carries the canonical `Value`, the producer-reported quality and
 * an optional source timestamp from the original data source. Writer APIs take
 * this type by value so runtime code can move the payload into the hot path.
 */
struct UpdateRequest {
  Value value;
  Quality quality{Quality::Good};
  std::optional<Timestamp> source_timestamp;
};

}  // namespace gt::datahub
