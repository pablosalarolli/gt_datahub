#pragma once

#include "gt_datahub/errors.hpp"
#include "gt_datahub/variable_definition.hpp"
#include "gt_datahub/variable_state.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gt::datahub {

/**
 * Read-only public view of the DataHub state and metadata.
 *
 * This interface is the application-facing contract for reading definitions,
 * effective state snapshots and resolved text expressions.
 */
class IDataHub {
 public:
  virtual ~IDataHub() = default;

  /**
   * Returns the effective public state for a variable when it exists.
   */
  virtual std::optional<VariableState> getState(
      std::string_view variable_name) const = 0;

  /**
   * Returns the public definition for a variable when it exists.
   */
  virtual std::optional<VariableDefinition> getDefinition(
      std::string_view variable_name) const = 0;

  /**
   * Lists the variables currently known by the hub.
   */
  virtual std::vector<std::string> listVariables() const = 0;

  /**
   * Resolves selectors and templates against the public read context.
   */
  virtual std::expected<std::string, ResolveError> resolveText(
      std::string_view expression) const = 0;
};

}  // namespace gt::datahub
