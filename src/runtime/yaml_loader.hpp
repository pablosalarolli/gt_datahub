#pragma once

#include "gt_datahub/data_type.hpp"
#include "gt_datahub/timestamp.hpp"
#include "gt_datahub/value.hpp"
#include "gt_datahub/variable_definition.hpp"
#include "gt_datahub/variable_role.hpp"

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gt::datahub::runtime {

/**
 * Error categories returned when the YAML configuration cannot be loaded.
 */
enum class YamlLoadErrorCode {
  InvalidYaml,
  InvalidSchema,
  MissingRequiredField,
  InvalidFieldType,
  InvalidValue,
  DuplicateConnectorId,
  DuplicateVariableName
};

/**
 * Internal error payload produced by the YAML loader.
 */
struct YamlLoadError {
  YamlLoadErrorCode code{YamlLoadErrorCode::InvalidYaml};
  std::string message;
};

/**
 * Raw connector configuration loaded from YAML before later compilation
 * phases derive kind-specific binding/runtime structures.
 */
struct ConnectorConfig {
  std::string id;
  std::string kind;
  bool enabled{true};
  YAML::Node settings{YAML::NodeType::Map};
};

/**
 * YAML-backed configuration materialized by sprint 3.1.
 *
 * This stage only loads `connectors` and `variables`; later sprints will
 * extend the structure with bindings, exports and compiled runtime metadata.
 */
struct LoadedDatahubConfig {
  int schema_version{1};
  std::vector<ConnectorConfig> connectors;
  std::vector<VariableDefinition> variables;
};

/**
 * Minimal YAML loader for bootstrap configuration.
 *
 * The loader intentionally focuses on the sprint 3.1 surface: schema version,
 * `connectors` and `variables`, including uniqueness checks for connector IDs
 * and variable names.
 */
class YamlLoader {
 public:
  /**
   * Loads a DataHub configuration document from a YAML string.
   */
  static std::expected<LoadedDatahubConfig, YamlLoadError> loadFromString(
      std::string_view yaml_text) {
    try {
      return loadFromRoot(YAML::Load(std::string(yaml_text)));
    } catch (const YAML::Exception& ex) {
      return std::unexpected(
          YamlLoadError{YamlLoadErrorCode::InvalidYaml, ex.what()});
    }
  }

  /**
   * Loads a DataHub configuration document from a YAML file path.
   */
  static std::expected<LoadedDatahubConfig, YamlLoadError> loadFromFile(
      std::string_view yaml_path) {
    try {
      return loadFromRoot(YAML::LoadFile(std::string(yaml_path)));
    } catch (const YAML::Exception& ex) {
      return std::unexpected(
          YamlLoadError{YamlLoadErrorCode::InvalidYaml, ex.what()});
    }
  }

 private:
  static std::expected<LoadedDatahubConfig, YamlLoadError> loadFromRoot(
      const YAML::Node& root) {
    if (!root || !root.IsMap()) {
      return invalidSchema("root document must be a YAML mapping");
    }

    const YAML::Node datahub = root["datahub"];
    if (!datahub || !datahub.IsMap()) {
      return invalidSchema("missing required `datahub` mapping");
    }

    const auto schema_version = parseRequiredInt(datahub, "schema_version",
                                                 "datahub.schema_version");
    if (!schema_version.has_value()) {
      return std::unexpected(schema_version.error());
    }

    if (*schema_version != 1) {
      return invalidSchema(
          "`datahub.schema_version` must be exactly 1 in baseline v3.7.2");
    }

    LoadedDatahubConfig config;
    config.schema_version = *schema_version;

    const auto connectors =
        parseConnectors(datahub["connectors"], "datahub.connectors");
    if (!connectors.has_value()) {
      return std::unexpected(connectors.error());
    }
    config.connectors = std::move(*connectors);

    const auto variables =
        parseVariables(datahub["variables"], "datahub.variables");
    if (!variables.has_value()) {
      return std::unexpected(variables.error());
    }
    config.variables = std::move(*variables);

    return config;
  }

  static std::expected<std::vector<ConnectorConfig>, YamlLoadError>
  parseConnectors(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::vector<ConnectorConfig>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<ConnectorConfig> connectors;
    connectors.reserve(node.size());

    std::unordered_set<std::string> seen_ids;
    seen_ids.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto connector = parseConnector(node[i], item_path);
      if (!connector.has_value()) {
        return std::unexpected(connector.error());
      }

      if (!seen_ids.insert(connector->id).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::DuplicateConnectorId,
            "duplicate connector id: " + connector->id});
      }

      connectors.push_back(std::move(*connector));
    }

    return connectors;
  }

  static std::expected<ConnectorConfig, YamlLoadError> parseConnector(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    ConnectorConfig connector;

    const auto id = parseRequiredString(node, "id", childPath(field_path, "id"));
    if (!id.has_value()) {
      return std::unexpected(id.error());
    }
    connector.id = std::move(*id);

    const auto kind =
        parseRequiredString(node, "kind", childPath(field_path, "kind"));
    if (!kind.has_value()) {
      return std::unexpected(kind.error());
    }
    connector.kind = std::move(*kind);

    const auto enabled =
        parseOptionalBool(node, "enabled", childPath(field_path, "enabled"));
    if (!enabled.has_value()) {
      return std::unexpected(enabled.error());
    }
    connector.enabled = enabled->value_or(true);

    const YAML::Node settings = node["settings"];
    if (settings) {
      if (!settings.IsMap()) {
        return invalidFieldType(childPath(field_path, "settings"),
                                "must be a YAML mapping");
      }
      connector.settings = settings;
    }

    return connector;
  }

  static std::expected<std::vector<VariableDefinition>, YamlLoadError>
  parseVariables(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::vector<VariableDefinition>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<VariableDefinition> variables;
    variables.reserve(node.size());

    std::unordered_set<std::string> seen_names;
    seen_names.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto variable = parseVariable(node[i], item_path);
      if (!variable.has_value()) {
        return std::unexpected(variable.error());
      }

      if (!seen_names.insert(variable->name).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::DuplicateVariableName,
            "duplicate variable name: " + variable->name});
      }

      variables.push_back(std::move(*variable));
    }

    return variables;
  }

  static std::expected<VariableDefinition, YamlLoadError> parseVariable(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    VariableDefinition definition;

    const auto name =
        parseRequiredString(node, "name", childPath(field_path, "name"));
    if (!name.has_value()) {
      return std::unexpected(name.error());
    }
    definition.name = std::move(*name);

    const auto data_type = parseDataType(
        node["data_type"], childPath(field_path, "data_type"));
    if (!data_type.has_value()) {
      return std::unexpected(data_type.error());
    }
    definition.data_type = *data_type;

    const auto role =
        parseOptionalRole(node["role"], childPath(field_path, "role"));
    if (!role.has_value()) {
      return std::unexpected(role.error());
    }
    definition.role = role->value_or(VariableRole::Other);

    const auto description = parseOptionalString(
        node["description"], childPath(field_path, "description"));
    if (!description.has_value()) {
      return std::unexpected(description.error());
    }
    definition.description = std::move(description->value_or(std::string{}));

    const auto unit =
        parseOptionalString(node["unit"], childPath(field_path, "unit"));
    if (!unit.has_value()) {
      return std::unexpected(unit.error());
    }
    definition.unit = std::move(unit->value_or(std::string{}));

    const auto groups =
        parseOptionalStringList(node["groups"], childPath(field_path, "groups"));
    if (!groups.has_value()) {
      return std::unexpected(groups.error());
    }
    definition.groups = std::move(groups->value_or(std::vector<std::string>{}));

    const auto labels =
        parseOptionalStringList(node["labels"], childPath(field_path, "labels"));
    if (!labels.has_value()) {
      return std::unexpected(labels.error());
    }
    definition.labels = std::move(labels->value_or(std::vector<std::string>{}));

    const auto default_value = parseOptionalValue(
        node["default_value"], definition.data_type,
        childPath(field_path, "default_value"));
    if (!default_value.has_value()) {
      return std::unexpected(default_value.error());
    }
    definition.default_value = std::move(*default_value);

    const auto min_value = parseOptionalValue(
        node["min_value"], definition.data_type, childPath(field_path, "min_value"));
    if (!min_value.has_value()) {
      return std::unexpected(min_value.error());
    }
    definition.min_value = std::move(*min_value);

    const auto max_value = parseOptionalValue(
        node["max_value"], definition.data_type, childPath(field_path, "max_value"));
    if (!max_value.has_value()) {
      return std::unexpected(max_value.error());
    }
    definition.max_value = std::move(*max_value);

    const auto precision =
        parseOptionalInt(node, "precision", childPath(field_path, "precision"));
    if (!precision.has_value()) {
      return std::unexpected(precision.error());
    }
    definition.precision = *precision;

    const auto historize =
        parseOptionalBool(node, "historize", childPath(field_path, "historize"));
    if (!historize.has_value()) {
      return std::unexpected(historize.error());
    }
    definition.historize = historize->value_or(false);

    const auto stale_after_ms = parseOptionalInt(
        node, "stale_after_ms", childPath(field_path, "stale_after_ms"));
    if (!stale_after_ms.has_value()) {
      return std::unexpected(stale_after_ms.error());
    }
    if (stale_after_ms->has_value()) {
      if (**stale_after_ms <= 0) {
        return invalidValue(childPath(field_path, "stale_after_ms"),
                            "must be positive when provided");
      }
      definition.stale_after_ms = std::chrono::milliseconds{**stale_after_ms};
    }

    return definition;
  }

  static std::expected<DataType, YamlLoadError> parseDataType(
      const YAML::Node& node, std::string_view field_path) {
    const auto value = parseRequiredScalar(node, field_path);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }

    if (*value == "Bool") {
      return DataType::Bool;
    }
    if (*value == "Int32") {
      return DataType::Int32;
    }
    if (*value == "UInt32") {
      return DataType::UInt32;
    }
    if (*value == "Int64") {
      return DataType::Int64;
    }
    if (*value == "UInt64") {
      return DataType::UInt64;
    }
    if (*value == "Float") {
      return DataType::Float;
    }
    if (*value == "Double") {
      return DataType::Double;
    }
    if (*value == "String") {
      return DataType::String;
    }
    if (*value == "DateTime") {
      return DataType::DateTime;
    }

    return invalidValue(field_path, "unsupported data_type: " + *value);
  }

  static std::expected<std::optional<VariableRole>, YamlLoadError> parseOptionalRole(
      const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<VariableRole>{};
    }

    const auto value = parseRequiredScalar(node, field_path);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }

    if (*value == "State") {
      return std::optional<VariableRole>{VariableRole::State};
    }
    if (*value == "Measurement") {
      return std::optional<VariableRole>{VariableRole::Measurement};
    }
    if (*value == "Alarm") {
      return std::optional<VariableRole>{VariableRole::Alarm};
    }
    if (*value == "Command") {
      return std::optional<VariableRole>{VariableRole::Command};
    }
    if (*value == "Calculated") {
      return std::optional<VariableRole>{VariableRole::Calculated};
    }
    if (*value == "Other") {
      return std::optional<VariableRole>{VariableRole::Other};
    }

    return invalidValue(field_path, "unsupported role: " + *value);
  }

  static std::expected<std::optional<Value>, YamlLoadError> parseOptionalValue(
      const YAML::Node& node, DataType data_type, std::string_view field_path) {
    if (!node) {
      return std::optional<Value>{};
    }

    const auto value = parseValue(node, data_type, field_path);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }

    return std::optional<Value>{std::move(*value)};
  }

  static std::expected<Value, YamlLoadError> parseValue(
      const YAML::Node& node, DataType data_type, std::string_view field_path) {
    try {
      switch (data_type) {
        case DataType::Bool:
          return Value{node.as<bool>()};
        case DataType::Int32:
          return Value{node.as<std::int32_t>()};
        case DataType::UInt32:
          return Value{node.as<std::uint32_t>()};
        case DataType::Int64:
          return Value{node.as<std::int64_t>()};
        case DataType::UInt64:
          return Value{node.as<std::uint64_t>()};
        case DataType::Float:
          return Value{node.as<float>()};
        case DataType::Double:
          return Value{node.as<double>()};
        case DataType::String:
          return Value{node.as<std::string>()};
        case DataType::DateTime: {
          const auto text = parseRequiredScalar(node, field_path);
          if (!text.has_value()) {
            return std::unexpected(text.error());
          }
          const auto timestamp = parseIso8601Utc(*text, field_path);
          if (!timestamp.has_value()) {
            return std::unexpected(timestamp.error());
          }
          return Value{*timestamp};
        }
      }
    } catch (const YAML::Exception&) {
      return invalidValue(field_path,
                          "value is incompatible with declared data_type");
    }

    return invalidValue(field_path, "unsupported data_type");
  }

  static std::expected<Timestamp, YamlLoadError> parseIso8601Utc(
      std::string_view text, std::string_view field_path) {
    if (text.size() != 20 && text.size() != 24) {
      return invalidValue(field_path,
                          "DateTime must use ISO-8601 UTC format");
    }

    if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':') {
      return invalidValue(field_path,
                          "DateTime must use ISO-8601 UTC format");
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;

    if (!parseDigits(text.substr(0, 4), year) ||
        !parseDigits(text.substr(5, 2), month) ||
        !parseDigits(text.substr(8, 2), day) ||
        !parseDigits(text.substr(11, 2), hour) ||
        !parseDigits(text.substr(14, 2), minute) ||
        !parseDigits(text.substr(17, 2), second)) {
      return invalidValue(field_path,
                          "DateTime must use ISO-8601 UTC format");
    }

    if (text.size() == 20) {
      if (text[19] != 'Z') {
        return invalidValue(field_path,
                            "DateTime must use ISO-8601 UTC format");
      }
    } else {
      if (text[19] != '.' || text[23] != 'Z' ||
          !parseDigits(text.substr(20, 3), millisecond)) {
        return invalidValue(field_path,
                            "DateTime must use ISO-8601 UTC format");
      }
    }

    if (hour > 23 || minute > 59 || second > 59) {
      return invalidValue(field_path,
                          "DateTime contains an invalid time component");
    }

    const auto ymd = std::chrono::year_month_day{
        std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}};
    if (!ymd.ok()) {
      return invalidValue(field_path,
                          "DateTime contains an invalid calendar date");
    }

    const auto tp = std::chrono::sys_days{ymd} + std::chrono::hours{hour} +
                    std::chrono::minutes{minute} +
                    std::chrono::seconds{second} +
                    std::chrono::milliseconds{millisecond};
    return Timestamp{tp.time_since_epoch()};
  }

  static bool parseDigits(std::string_view text, int& value) {
    if (text.empty()) {
      return false;
    }

    int parsed = 0;
    for (const char ch : text) {
      if (ch < '0' || ch > '9') {
        return false;
      }
      parsed = (parsed * 10) + (ch - '0');
    }

    value = parsed;
    return true;
  }

  static std::expected<std::string, YamlLoadError> parseRequiredString(
      const YAML::Node& node, std::string_view key, std::string_view field_path) {
    const YAML::Node value = node[std::string(key)];
    const auto scalar = parseRequiredScalar(value, field_path);
    if (!scalar.has_value()) {
      return std::unexpected(scalar.error());
    }

    if (scalar->empty()) {
      return invalidValue(field_path, "must not be empty");
    }

    return *scalar;
  }

  static std::expected<std::optional<std::string>, YamlLoadError>
  parseOptionalString(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<std::string>{};
    }

    const auto scalar = parseRequiredScalar(node, field_path);
    if (!scalar.has_value()) {
      return std::unexpected(scalar.error());
    }

    return std::optional<std::string>{std::move(*scalar)};
  }

  static std::expected<std::string, YamlLoadError> parseRequiredScalar(
      const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return missingRequiredField(field_path);
    }

    if (!node.IsScalar()) {
      return invalidFieldType(field_path, "must be a YAML scalar");
    }

    try {
      return node.as<std::string>();
    } catch (const YAML::Exception&) {
      return invalidFieldType(field_path, "must be convertible to string");
    }
  }

  static std::expected<std::optional<bool>, YamlLoadError> parseOptionalBool(
      const YAML::Node& node, std::string_view key, std::string_view field_path) {
    const YAML::Node value = node[std::string(key)];
    if (!value) {
      return std::optional<bool>{};
    }

    try {
      return std::optional<bool>{value.as<bool>()};
    } catch (const YAML::Exception&) {
      return invalidValue(field_path, "must be a boolean");
    }
  }

  static std::expected<std::optional<int>, YamlLoadError> parseOptionalInt(
      const YAML::Node& node, std::string_view key, std::string_view field_path) {
    const YAML::Node value = node[std::string(key)];
    if (!value) {
      return std::optional<int>{};
    }

    try {
      return std::optional<int>{value.as<int>()};
    } catch (const YAML::Exception&) {
      return invalidValue(field_path, "must be an integer");
    }
  }

  static std::expected<int, YamlLoadError> parseRequiredInt(
      const YAML::Node& node, std::string_view key, std::string_view field_path) {
    const YAML::Node value = node[std::string(key)];
    if (!value) {
      return missingRequiredField(field_path);
    }

    try {
      return value.as<int>();
    } catch (const YAML::Exception&) {
      return invalidValue(field_path, "must be an integer");
    }
  }

  static std::expected<std::optional<std::vector<std::string>>, YamlLoadError>
  parseOptionalStringList(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<std::vector<std::string>>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<std::string> values;
    values.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item = parseRequiredScalar(node[i], childPath(field_path, i));
      if (!item.has_value()) {
        return std::unexpected(item.error());
      }
      values.push_back(std::move(*item));
    }

    return std::optional<std::vector<std::string>>{std::move(values)};
  }

  static std::unexpected<YamlLoadError> invalidSchema(std::string message) {
    return std::unexpected(
        YamlLoadError{YamlLoadErrorCode::InvalidSchema, std::move(message)});
  }

  static std::unexpected<YamlLoadError> missingRequiredField(
      std::string_view field_path) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::MissingRequiredField,
        "missing required field: " + std::string(field_path)});
  }

  static std::unexpected<YamlLoadError> invalidFieldType(
      std::string_view field_path, std::string message) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::InvalidFieldType,
        std::string(field_path) + " " + std::move(message)});
  }

  static std::unexpected<YamlLoadError> invalidValue(
      std::string_view field_path, std::string message) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::InvalidValue,
        std::string(field_path) + " " + std::move(message)});
  }

  static std::string childPath(std::string_view base, std::string_view child) {
    return std::string(base) + "." + std::string(child);
  }

  static std::string childPath(std::string_view base, std::size_t index) {
    return std::string(base) + "[" + std::to_string(index) + "]";
  }
};

}  // namespace gt::datahub::runtime
