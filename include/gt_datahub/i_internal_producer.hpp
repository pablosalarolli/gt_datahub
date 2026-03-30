#pragma once

#include "gt_datahub/errors.hpp"
#include "gt_datahub/update_request.hpp"

#include <expected>
#include <string_view>

namespace gt::datahub {

/**
 * Public handle used by the application to publish values through an internal
 * producer binding.
 *
 * Each handle is tied to exactly one configured internal binding and one target
 * variable, preserving the baseline rule of one logical producer per variable.
 */
class IInternalProducer {
 public:
  virtual ~IInternalProducer() = default;

  /**
   * Returns the binding id that originated this writer handle.
   */
  virtual std::string_view bindingId() const noexcept = 0;

  /**
   * Returns the variable name owned by this writer handle.
   */
  virtual std::string_view variableName() const noexcept = 0;

  /**
   * Submits a producer update by value so runtime code can move the payload in
   * the hot path after validation.
   */
  virtual std::expected<void, SubmitError> submit(UpdateRequest req) = 0;
};

}  // namespace gt::datahub
