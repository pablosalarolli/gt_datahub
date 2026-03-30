#pragma once

namespace gt::datahub {

/**
 * Canonical value types accepted by the public DataHub contract.
 *
 * Variable definitions use this enum instead of free-form strings so the
 * compiled runtime keeps a typed contract after bootstrap.
 */
enum class DataType {
  Bool,
  Int32,
  UInt32,
  Int64,
  UInt64,
  Float,
  Double,
  String,
  DateTime
};

}  // namespace gt::datahub
