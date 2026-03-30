#pragma once

namespace gt::datahub {

/**
 * Operational role metadata attached to a variable definition.
 *
 * The role informs how a variable is classified by applications and tooling,
 * without changing the ownership rules of the runtime.
 */
enum class VariableRole {
  State,
  Measurement,
  Alarm,
  Command,
  Calculated,
  Other
};

}  // namespace gt::datahub
