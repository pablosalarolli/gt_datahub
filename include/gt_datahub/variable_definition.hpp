#pragma once

#include "gt_datahub/data_type.hpp"
#include "gt_datahub/value.hpp"
#include "gt_datahub/variable_role.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace gt::datahub {

/**
 * Public variable metadata and bootstrap policy exposed by the DataHub.
 *
 * `default_value` is not decorative metadata: it defines the initial public
 * state materialized during bootstrap when present.
 */
struct VariableDefinition {
  std::string name;
  DataType data_type;
  VariableRole role{VariableRole::Other};
  std::string description;
  std::string unit;
  std::vector<std::string> groups;
  std::vector<std::string> labels;
  std::optional<Value> default_value;
  std::optional<Value> min_value;
  std::optional<Value> max_value;
  std::optional<int> precision;
  bool historize{false};
  std::optional<std::chrono::milliseconds> stale_after_ms;
};

}  // namespace gt::datahub
