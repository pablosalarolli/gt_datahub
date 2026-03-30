#pragma once

#include "gt_datahub/errors.hpp"
#include "gt_datahub/quality.hpp"
#include "gt_datahub/timestamp.hpp"
#include "gt_datahub/value.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>

namespace gt::datahub::runtime {

/**
 * Immutable token that binds one runtime producer to one configured variable.
 */
struct ProducerToken {
  std::size_t binding_index;
  std::size_t variable_index;
};

/**
 * Internal write payload accepted by runtime-owned producers and adapters.
 *
 * `synthetic` is reserved for runtime-originated degradation updates and stays
 * explicit in the type even before connector adapters start using it.
 */
struct RuntimeUpdateRequest {
  Value value;
  Quality quality{Quality::Good};
  std::optional<Timestamp> source_timestamp;
  bool synthetic{false};
};

/**
 * Internal mutable access contract used by adapters and internal producer
 * handles.
 */
class IRuntimeHubAccess {
 public:
  virtual ~IRuntimeHubAccess() = default;

  /**
   * Submits one update through a validated producer token.
   */
  virtual std::expected<void, SubmitError> submitFromProducer(
      ProducerToken token, RuntimeUpdateRequest req) = 0;

  /**
   * Marks one producer as degraded without exposing that path publicly.
   */
  virtual void markProducerConnectionBad(ProducerToken token,
                                         std::string_view reason) = 0;
};

}  // namespace gt::datahub::runtime
