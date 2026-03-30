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
  DuplicateVariableName,
  DuplicateProducerBindingId,
  DuplicateConsumerBindingId,
  DuplicateFileExportId,
  UnknownConnector,
  UnknownVariable,
  InvalidConnectorCapability,
  InvalidProducerOwnership
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
 * Logical producer ownership declared in YAML.
 */
enum class ProducerKind { Internal, Connector };

/**
 * Uncompiled acquisition metadata kept from YAML until runtime wiring.
 */
struct AcquisitionConfig {
  std::string mode;
  YAML::Node raw_node{YAML::NodeType::Map};
};

/**
 * Uncompiled external binding metadata kept from YAML until adapter setup.
 */
struct BindingConfig {
  std::string type;
  YAML::Node raw_node{YAML::NodeType::Map};
};

/**
 * Producer binding definition loaded from YAML.
 */
struct ProducerBindingConfig {
  std::string id;
  std::string variable_name;
  ProducerKind producer_kind{ProducerKind::Internal};
  std::optional<std::string> connector_id;
  bool enabled{true};
  std::optional<AcquisitionConfig> acquisition;
  std::optional<BindingConfig> binding;
};

/**
 * Consumer trigger metadata kept in raw form until runtime compilation.
 */
struct ConsumerTriggerConfig {
  std::string mode;
  YAML::Node raw_node{YAML::NodeType::Map};
};

/**
 * Consumer binding definition loaded from YAML.
 */
struct ConsumerBindingConfig {
  std::string id;
  std::string variable_name;
  std::string connector_id;
  bool enabled{true};
  ConsumerTriggerConfig trigger;
  BindingConfig binding;
};

/**
 * Export trigger metadata kept in raw form until runtime compilation.
 */
struct FileExportTriggerConfig {
  std::string mode;
  std::optional<std::chrono::milliseconds> period_ms;
  YAML::Node raw_node{YAML::NodeType::Map};
};

/**
 * Export column definition before selector/template compilation.
 */
struct ExportColumnConfig {
  std::string name;
  std::optional<std::string> source;
  std::optional<std::string> expression;
};

/**
 * File export definition loaded from YAML.
 *
 * `activation` remains as raw YAML for sprint 3.3, when predicate compilation
 * is introduced. `columns` are parsed structurally here so the loader already
 * enforces the baseline shape of the export contract.
 */
struct FileExportConfig {
  std::string id;
  bool enabled{true};
  std::string connector_id;
  std::string format;
  std::string target_template;
  bool append{false};
  bool write_header_if_missing{false};
  FileExportTriggerConfig trigger;
  YAML::Node activation;
  std::vector<ExportColumnConfig> columns;
};

/**
 * YAML-backed configuration materialized by the current bootstrap phase.
 *
 * At sprint 3.2 the loader owns the structural parsing of connectors,
 * variables, bindings and exports. Selectors, templates and predicates remain
 * uncompiled and will be handled in sprint 3.3.
 */
struct LoadedDatahubConfig {
  int schema_version{1};
  std::vector<ConnectorConfig> connectors;
  std::vector<VariableDefinition> variables;
  std::vector<ProducerBindingConfig> producer_bindings;
  std::vector<ConsumerBindingConfig> consumer_bindings;
  std::vector<FileExportConfig> file_exports;
};

/**
 * Minimal YAML loader for bootstrap configuration.
 *
 * The loader now covers the full structural surface needed by phase 3 sprint
 * 3.2: connectors, variables, bindings and file exports, plus the early
 * validation rules that must fail before runtime startup.
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

    const auto producer_bindings = parseProducerBindings(
        datahub["producer_bindings"], "datahub.producer_bindings");
    if (!producer_bindings.has_value()) {
      return std::unexpected(producer_bindings.error());
    }
    config.producer_bindings = std::move(*producer_bindings);

    const auto consumer_bindings = parseConsumerBindings(
        datahub["consumer_bindings"], "datahub.consumer_bindings");
    if (!consumer_bindings.has_value()) {
      return std::unexpected(consumer_bindings.error());
    }
    config.consumer_bindings = std::move(*consumer_bindings);

    const auto file_exports =
        parseFileExports(datahub["file_exports"], "datahub.file_exports");
    if (!file_exports.has_value()) {
      return std::unexpected(file_exports.error());
    }
    config.file_exports = std::move(*file_exports);

    const auto validation = validateBindingsAndExports(config);
    if (!validation.has_value()) {
      return std::unexpected(validation.error());
    }

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
        node["min_value"], definition.data_type,
        childPath(field_path, "min_value"));
    if (!min_value.has_value()) {
      return std::unexpected(min_value.error());
    }
    definition.min_value = std::move(*min_value);

    const auto max_value = parseOptionalValue(
        node["max_value"], definition.data_type,
        childPath(field_path, "max_value"));
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

  static std::expected<std::vector<ProducerBindingConfig>, YamlLoadError>
  parseProducerBindings(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::vector<ProducerBindingConfig>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<ProducerBindingConfig> bindings;
    bindings.reserve(node.size());

    std::unordered_set<std::string> seen_ids;
    seen_ids.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto binding = parseProducerBinding(node[i], item_path);
      if (!binding.has_value()) {
        return std::unexpected(binding.error());
      }

      if (!seen_ids.insert(binding->id).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::DuplicateProducerBindingId,
            "duplicate producer binding id: " + binding->id});
      }

      bindings.push_back(std::move(*binding));
    }

    return bindings;
  }

  static std::expected<ProducerBindingConfig, YamlLoadError> parseProducerBinding(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    ProducerBindingConfig binding;

    const auto id = parseRequiredString(node, "id", childPath(field_path, "id"));
    if (!id.has_value()) {
      return std::unexpected(id.error());
    }
    binding.id = std::move(*id);

    const auto variable_name = parseRequiredString(
        node, "variable_name", childPath(field_path, "variable_name"));
    if (!variable_name.has_value()) {
      return std::unexpected(variable_name.error());
    }
    binding.variable_name = std::move(*variable_name);

    const auto producer_kind = parseProducerKind(
        node["producer_kind"], childPath(field_path, "producer_kind"));
    if (!producer_kind.has_value()) {
      return std::unexpected(producer_kind.error());
    }
    binding.producer_kind = *producer_kind;

    const auto connector_id = parseOptionalString(
        node["connector_id"], childPath(field_path, "connector_id"));
    if (!connector_id.has_value()) {
      return std::unexpected(connector_id.error());
    }
    binding.connector_id = std::move(*connector_id);

    const auto enabled =
        parseOptionalBool(node, "enabled", childPath(field_path, "enabled"));
    if (!enabled.has_value()) {
      return std::unexpected(enabled.error());
    }
    binding.enabled = enabled->value_or(true);

    const auto acquisition = parseOptionalAcquisition(
        node["acquisition"], childPath(field_path, "acquisition"));
    if (!acquisition.has_value()) {
      return std::unexpected(acquisition.error());
    }
    binding.acquisition = std::move(*acquisition);

    const auto external_binding = parseOptionalBinding(
        node["binding"], childPath(field_path, "binding"));
    if (!external_binding.has_value()) {
      return std::unexpected(external_binding.error());
    }
    binding.binding = std::move(*external_binding);

    if (binding.producer_kind == ProducerKind::Internal) {
      if (binding.connector_id.has_value()) {
        return invalidValue(childPath(field_path, "connector_id"),
                            "must be absent when `producer_kind` is `internal`");
      }

      if (binding.acquisition.has_value()) {
        return invalidValue(childPath(field_path, "acquisition"),
                            "must be absent when `producer_kind` is `internal`");
      }

      if (binding.binding.has_value()) {
        return invalidValue(childPath(field_path, "binding"),
                            "must be absent when `producer_kind` is `internal`");
      }
    } else {
      if (!binding.connector_id.has_value()) {
        return missingRequiredField(childPath(field_path, "connector_id"));
      }

      if (!binding.binding.has_value()) {
        return missingRequiredField(childPath(field_path, "binding"));
      }
    }

    return binding;
  }

  static std::expected<std::vector<ConsumerBindingConfig>, YamlLoadError>
  parseConsumerBindings(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::vector<ConsumerBindingConfig>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<ConsumerBindingConfig> bindings;
    bindings.reserve(node.size());

    std::unordered_set<std::string> seen_ids;
    seen_ids.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto binding = parseConsumerBinding(node[i], item_path);
      if (!binding.has_value()) {
        return std::unexpected(binding.error());
      }

      if (!seen_ids.insert(binding->id).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::DuplicateConsumerBindingId,
            "duplicate consumer binding id: " + binding->id});
      }

      bindings.push_back(std::move(*binding));
    }

    return bindings;
  }

  static std::expected<ConsumerBindingConfig, YamlLoadError> parseConsumerBinding(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    ConsumerBindingConfig binding;

    const auto id = parseRequiredString(node, "id", childPath(field_path, "id"));
    if (!id.has_value()) {
      return std::unexpected(id.error());
    }
    binding.id = std::move(*id);

    const auto variable_name = parseRequiredString(
        node, "variable_name", childPath(field_path, "variable_name"));
    if (!variable_name.has_value()) {
      return std::unexpected(variable_name.error());
    }
    binding.variable_name = std::move(*variable_name);

    const auto connector_id = parseRequiredString(
        node, "connector_id", childPath(field_path, "connector_id"));
    if (!connector_id.has_value()) {
      return std::unexpected(connector_id.error());
    }
    binding.connector_id = std::move(*connector_id);

    const auto enabled =
        parseOptionalBool(node, "enabled", childPath(field_path, "enabled"));
    if (!enabled.has_value()) {
      return std::unexpected(enabled.error());
    }
    binding.enabled = enabled->value_or(true);

    const auto trigger = parseConsumerTrigger(
        node["trigger"], childPath(field_path, "trigger"));
    if (!trigger.has_value()) {
      return std::unexpected(trigger.error());
    }
    binding.trigger = std::move(*trigger);

    const auto external_binding =
        parseRequiredBinding(node["binding"], childPath(field_path, "binding"));
    if (!external_binding.has_value()) {
      return std::unexpected(external_binding.error());
    }
    binding.binding = std::move(*external_binding);

    return binding;
  }

  static std::expected<std::vector<FileExportConfig>, YamlLoadError>
  parseFileExports(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::vector<FileExportConfig>{};
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<FileExportConfig> exports;
    exports.reserve(node.size());

    std::unordered_set<std::string> seen_ids;
    seen_ids.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto export_config = parseFileExport(node[i], item_path);
      if (!export_config.has_value()) {
        return std::unexpected(export_config.error());
      }

      if (!seen_ids.insert(export_config->id).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::DuplicateFileExportId,
            "duplicate file export id: " + export_config->id});
      }

      exports.push_back(std::move(*export_config));
    }

    return exports;
  }

  static std::expected<FileExportConfig, YamlLoadError> parseFileExport(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    FileExportConfig export_config;

    const auto id = parseRequiredString(node, "id", childPath(field_path, "id"));
    if (!id.has_value()) {
      return std::unexpected(id.error());
    }
    export_config.id = std::move(*id);

    const auto enabled =
        parseOptionalBool(node, "enabled", childPath(field_path, "enabled"));
    if (!enabled.has_value()) {
      return std::unexpected(enabled.error());
    }
    export_config.enabled = enabled->value_or(true);

    const auto connector_id = parseRequiredString(
        node, "connector_id", childPath(field_path, "connector_id"));
    if (!connector_id.has_value()) {
      return std::unexpected(connector_id.error());
    }
    export_config.connector_id = std::move(*connector_id);

    const auto format =
        parseRequiredString(node, "format", childPath(field_path, "format"));
    if (!format.has_value()) {
      return std::unexpected(format.error());
    }
    export_config.format = std::move(*format);

    const auto target_template = parseRequiredString(
        node, "target_template", childPath(field_path, "target_template"));
    if (!target_template.has_value()) {
      return std::unexpected(target_template.error());
    }
    export_config.target_template = std::move(*target_template);

    const auto append =
        parseOptionalBool(node, "append", childPath(field_path, "append"));
    if (!append.has_value()) {
      return std::unexpected(append.error());
    }
    export_config.append = append->value_or(false);

    const auto write_header_if_missing = parseOptionalBool(
        node, "write_header_if_missing",
        childPath(field_path, "write_header_if_missing"));
    if (!write_header_if_missing.has_value()) {
      return std::unexpected(write_header_if_missing.error());
    }
    export_config.write_header_if_missing =
        write_header_if_missing->value_or(false);

    const auto trigger = parseFileExportTrigger(
        node["trigger"], childPath(field_path, "trigger"));
    if (!trigger.has_value()) {
      return std::unexpected(trigger.error());
    }
    export_config.trigger = std::move(*trigger);

    const YAML::Node activation = node["activation"];
    if (activation) {
      if (!activation.IsMap()) {
        return invalidFieldType(childPath(field_path, "activation"),
                                "must be a YAML mapping");
      }
      export_config.activation = activation;
    }

    const auto columns =
        parseExportColumns(node["columns"], childPath(field_path, "columns"));
    if (!columns.has_value()) {
      return std::unexpected(columns.error());
    }
    export_config.columns = std::move(*columns);

    return export_config;
  }

  static std::expected<ProducerKind, YamlLoadError> parseProducerKind(
      const YAML::Node& node, std::string_view field_path) {
    const auto value = parseRequiredScalar(node, field_path);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }

    if (*value == "internal") {
      return ProducerKind::Internal;
    }
    if (*value == "connector") {
      return ProducerKind::Connector;
    }

    return invalidValue(field_path, "unsupported producer_kind: " + *value);
  }

  static std::expected<std::optional<AcquisitionConfig>, YamlLoadError>
  parseOptionalAcquisition(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<AcquisitionConfig>{};
    }

    if (!node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    AcquisitionConfig acquisition;
    const auto mode =
        parseRequiredString(node, "mode", childPath(field_path, "mode"));
    if (!mode.has_value()) {
      return std::unexpected(mode.error());
    }

    acquisition.mode = std::move(*mode);
    acquisition.raw_node = node;
    return std::optional<AcquisitionConfig>{std::move(acquisition)};
  }

  static std::expected<std::optional<BindingConfig>, YamlLoadError>
  parseOptionalBinding(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<BindingConfig>{};
    }

    const auto binding = parseRequiredBinding(node, field_path);
    if (!binding.has_value()) {
      return std::unexpected(binding.error());
    }

    return std::optional<BindingConfig>{std::move(*binding)};
  }

  static std::expected<BindingConfig, YamlLoadError> parseRequiredBinding(
      const YAML::Node& node, std::string_view field_path) {
    const auto raw_node = parseRequiredMap(node, field_path);
    if (!raw_node.has_value()) {
      return std::unexpected(raw_node.error());
    }

    BindingConfig binding;
    const auto type =
        parseRequiredString(*raw_node, "type", childPath(field_path, "type"));
    if (!type.has_value()) {
      return std::unexpected(type.error());
    }

    binding.type = std::move(*type);
    binding.raw_node = *raw_node;
    return binding;
  }

  static std::expected<ConsumerTriggerConfig, YamlLoadError> parseConsumerTrigger(
      const YAML::Node& node, std::string_view field_path) {
    const auto raw_node = parseRequiredMap(node, field_path);
    if (!raw_node.has_value()) {
      return std::unexpected(raw_node.error());
    }

    ConsumerTriggerConfig trigger;
    const auto mode =
        parseRequiredString(*raw_node, "mode", childPath(field_path, "mode"));
    if (!mode.has_value()) {
      return std::unexpected(mode.error());
    }

    trigger.mode = std::move(*mode);
    trigger.raw_node = *raw_node;
    return trigger;
  }

  static std::expected<FileExportTriggerConfig, YamlLoadError>
  parseFileExportTrigger(const YAML::Node& node, std::string_view field_path) {
    const auto raw_node = parseRequiredMap(node, field_path);
    if (!raw_node.has_value()) {
      return std::unexpected(raw_node.error());
    }

    FileExportTriggerConfig trigger;
    const auto mode =
        parseRequiredString(*raw_node, "mode", childPath(field_path, "mode"));
    if (!mode.has_value()) {
      return std::unexpected(mode.error());
    }

    trigger.mode = std::move(*mode);
    trigger.raw_node = *raw_node;

    const auto period_ms = parseOptionalInt(*raw_node, "period_ms",
                                            childPath(field_path, "period_ms"));
    if (!period_ms.has_value()) {
      return std::unexpected(period_ms.error());
    }

    if (period_ms->has_value()) {
      if (**period_ms <= 0) {
        return invalidValue(childPath(field_path, "period_ms"),
                            "must be positive when provided");
      }
      trigger.period_ms = std::chrono::milliseconds{**period_ms};
    }

    return trigger;
  }

  static std::expected<std::vector<ExportColumnConfig>, YamlLoadError>
  parseExportColumns(const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return missingRequiredField(field_path);
    }

    if (!node.IsSequence()) {
      return invalidFieldType(field_path, "must be a YAML sequence");
    }

    std::vector<ExportColumnConfig> columns;
    columns.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto item_path = childPath(field_path, i);
      auto column = parseExportColumn(node[i], item_path);
      if (!column.has_value()) {
        return std::unexpected(column.error());
      }

      columns.push_back(std::move(*column));
    }

    return columns;
  }

  static std::expected<ExportColumnConfig, YamlLoadError> parseExportColumn(
      const YAML::Node& node, std::string_view field_path) {
    if (!node || !node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    ExportColumnConfig column;

    const auto name =
        parseRequiredString(node, "name", childPath(field_path, "name"));
    if (!name.has_value()) {
      return std::unexpected(name.error());
    }
    column.name = std::move(*name);

    const auto source =
        parseOptionalString(node["source"], childPath(field_path, "source"));
    if (!source.has_value()) {
      return std::unexpected(source.error());
    }
    column.source = std::move(*source);

    const auto expression = parseOptionalString(
        node["expression"], childPath(field_path, "expression"));
    if (!expression.has_value()) {
      return std::unexpected(expression.error());
    }
    column.expression = std::move(*expression);

    const bool has_source = column.source.has_value();
    const bool has_expression = column.expression.has_value();
    if (has_source == has_expression) {
      return invalidValue(field_path,
                          "must declare exactly one of `source` or `expression`");
    }

    return column;
  }

  static std::expected<void, YamlLoadError> validateBindingsAndExports(
      const LoadedDatahubConfig& config) {
    const auto producers = validateProducerBindings(config);
    if (!producers.has_value()) {
      return std::unexpected(producers.error());
    }

    const auto consumers = validateConsumerBindings(config);
    if (!consumers.has_value()) {
      return std::unexpected(consumers.error());
    }

    const auto exports = validateFileExports(config);
    if (!exports.has_value()) {
      return std::unexpected(exports.error());
    }

    return {};
  }

  static std::expected<void, YamlLoadError> validateProducerBindings(
      const LoadedDatahubConfig& config) {
    std::unordered_set<std::string> active_producer_variables;
    active_producer_variables.reserve(config.producer_bindings.size());

    for (std::size_t i = 0; i < config.producer_bindings.size(); ++i) {
      const auto& binding = config.producer_bindings[i];
      const auto item_path = childPath("datahub.producer_bindings", i);

      if (!hasVariable(config, binding.variable_name)) {
        return unknownVariable(childPath(item_path, "variable_name"),
                               binding.variable_name);
      }

      if (binding.producer_kind == ProducerKind::Connector) {
        const ConnectorConfig* connector =
            findConnector(config, *binding.connector_id);
        if (connector == nullptr) {
          return unknownConnector(childPath(item_path, "connector_id"),
                                  *binding.connector_id);
        }

        if (!isProducerConnectorKindSupported(connector->kind)) {
          return invalidConnectorCapability(
              childPath(item_path, "connector_id"),
              "connector kind `" + connector->kind +
                  "` is not supported for producer bindings");
        }

        if (!isBindingTypeCompatibleWithKind(connector->kind,
                                             binding.binding->type)) {
          return invalidConnectorCapability(
              childPath(item_path, "binding.type"),
              "is not compatible with connector kind `" + connector->kind + "`");
        }

        if (binding.acquisition.has_value() &&
            !isProducerAcquisitionModeSupported(connector->kind,
                                                binding.acquisition->mode)) {
          return invalidConnectorCapability(
              childPath(item_path, "acquisition.mode"),
              "mode `" + binding.acquisition->mode +
                  "` is not supported by connector kind `" + connector->kind +
                  "`");
        }
      }

      // The baseline text says "exactly one" active producer, but the
      // consolidated spec YAML still contains sink/default-only variables.
      // Sprint 3.2 therefore enforces the safety invariant needed now: no
      // variable may have more than one active producer binding.
      if (binding.enabled &&
          !active_producer_variables.insert(binding.variable_name).second) {
        return std::unexpected(YamlLoadError{
            YamlLoadErrorCode::InvalidProducerOwnership,
            "more than one active producer binding targets variable: " +
                binding.variable_name});
      }
    }

    return {};
  }

  static std::expected<void, YamlLoadError> validateConsumerBindings(
      const LoadedDatahubConfig& config) {
    for (std::size_t i = 0; i < config.consumer_bindings.size(); ++i) {
      const auto& binding = config.consumer_bindings[i];
      const auto item_path = childPath("datahub.consumer_bindings", i);

      if (!hasVariable(config, binding.variable_name)) {
        return unknownVariable(childPath(item_path, "variable_name"),
                               binding.variable_name);
      }

      const ConnectorConfig* connector =
          findConnector(config, binding.connector_id);
      if (connector == nullptr) {
        return unknownConnector(childPath(item_path, "connector_id"),
                                binding.connector_id);
      }

      if (!isConsumerTriggerModeSupported(connector->kind, binding.trigger.mode)) {
        return invalidConnectorCapability(
            childPath(item_path, "trigger.mode"),
            "mode `" + binding.trigger.mode +
                "` is not supported by connector kind `" + connector->kind +
                "`");
      }

      if (!isBindingTypeCompatibleWithKind(connector->kind, binding.binding.type)) {
        return invalidConnectorCapability(
            childPath(item_path, "binding.type"),
            "is not compatible with connector kind `" + connector->kind + "`");
      }
    }

    return {};
  }

  static std::expected<void, YamlLoadError> validateFileExports(
      const LoadedDatahubConfig& config) {
    for (std::size_t i = 0; i < config.file_exports.size(); ++i) {
      const auto& export_config = config.file_exports[i];
      const auto item_path = childPath("datahub.file_exports", i);

      const ConnectorConfig* connector =
          findConnector(config, export_config.connector_id);
      if (connector == nullptr) {
        return unknownConnector(childPath(item_path, "connector_id"),
                                export_config.connector_id);
      }

      if (connector->kind != "file") {
        return invalidConnectorCapability(
            childPath(item_path, "connector_id"),
            "must reference a connector with kind `file`");
      }

      if (export_config.format != "csv") {
        return invalidValue(childPath(item_path, "format"),
                            "must be `csv` in baseline v3.7.2");
      }

      if (!isFileExportTriggerModeSupported(export_config.trigger.mode)) {
        return invalidValue(
            childPath(item_path, "trigger.mode"),
            "unsupported file export trigger mode: " + export_config.trigger.mode);
      }

      if (export_config.trigger.mode == "periodic" &&
          !export_config.trigger.period_ms.has_value()) {
        return missingRequiredField(childPath(item_path, "trigger.period_ms"));
      }
    }

    return {};
  }

  static bool hasVariable(const LoadedDatahubConfig& config,
                          std::string_view variable_name) noexcept {
    for (const auto& variable : config.variables) {
      if (variable.name == variable_name) {
        return true;
      }
    }

    return false;
  }

  static const ConnectorConfig* findConnector(
      const LoadedDatahubConfig& config,
      std::string_view connector_id) noexcept {
    for (const auto& connector : config.connectors) {
      if (connector.id == connector_id) {
        return &connector;
      }
    }

    return nullptr;
  }

  static bool isProducerConnectorKindSupported(
      std::string_view connector_kind) noexcept {
    return connector_kind == "opc_ua" || connector_kind == "opc_da" ||
           connector_kind == "file";
  }

  static bool isProducerAcquisitionModeSupported(
      std::string_view connector_kind, std::string_view mode) noexcept {
    if (connector_kind == "opc_ua") {
      return mode == "subscription" || mode == "polling";
    }
    if (connector_kind == "opc_da") {
      return mode == "polling";
    }
    if (connector_kind == "file") {
      return mode == "polling";
    }

    return false;
  }

  static bool isConsumerTriggerModeSupported(
      std::string_view connector_kind, std::string_view mode) noexcept {
    if (connector_kind == "opc_ua" || connector_kind == "opc_da") {
      return mode == "on_change";
    }

    return false;
  }

  static bool isFileExportTriggerModeSupported(
      std::string_view mode) noexcept {
    return mode == "periodic" || mode == "manual";
  }

  static bool isBindingTypeCompatibleWithKind(
      std::string_view connector_kind, std::string_view binding_type) noexcept {
    return hasPrefix(binding_type, connector_kind) &&
           binding_type.size() > connector_kind.size() &&
           binding_type[connector_kind.size()] == '.';
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

  static std::expected<std::optional<VariableRole>, YamlLoadError>
  parseOptionalRole(const YAML::Node& node, std::string_view field_path) {
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
      return invalidValue(field_path, "DateTime must use ISO-8601 UTC format");
    }

    if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':') {
      return invalidValue(field_path, "DateTime must use ISO-8601 UTC format");
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
      return invalidValue(field_path, "DateTime must use ISO-8601 UTC format");
    }

    if (text.size() == 20) {
      if (text[19] != 'Z') {
        return invalidValue(field_path,
                            "DateTime must use ISO-8601 UTC format");
      }
    } else if (text[19] != '.' || text[23] != 'Z' ||
               !parseDigits(text.substr(20, 3), millisecond)) {
      return invalidValue(field_path, "DateTime must use ISO-8601 UTC format");
    }

    if (hour > 23 || minute > 59 || second > 59) {
      return invalidValue(field_path,
                          "DateTime contains an invalid time component");
    }

    const auto ymd = std::chrono::year_month_day{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
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
    return parseOptionalBool(node[std::string(key)], field_path);
  }

  static std::expected<std::optional<bool>, YamlLoadError> parseOptionalBool(
      const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<bool>{};
    }

    try {
      return std::optional<bool>{node.as<bool>()};
    } catch (const YAML::Exception&) {
      return invalidValue(field_path, "must be a boolean");
    }
  }

  static std::expected<std::optional<int>, YamlLoadError> parseOptionalInt(
      const YAML::Node& node, std::string_view key, std::string_view field_path) {
    return parseOptionalInt(node[std::string(key)], field_path);
  }

  static std::expected<std::optional<int>, YamlLoadError> parseOptionalInt(
      const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return std::optional<int>{};
    }

    try {
      return std::optional<int>{node.as<int>()};
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

  static std::expected<YAML::Node, YamlLoadError> parseRequiredMap(
      const YAML::Node& node, std::string_view field_path) {
    if (!node) {
      return missingRequiredField(field_path);
    }

    if (!node.IsMap()) {
      return invalidFieldType(field_path, "must be a YAML mapping");
    }

    return node;
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

  static std::unexpected<YamlLoadError> unknownConnector(
      std::string_view field_path, std::string_view connector_id) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::UnknownConnector,
        std::string(field_path) + " references unknown connector: " +
            std::string(connector_id)});
  }

  static std::unexpected<YamlLoadError> unknownVariable(
      std::string_view field_path, std::string_view variable_name) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::UnknownVariable,
        std::string(field_path) + " references unknown variable: " +
            std::string(variable_name)});
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

  static std::unexpected<YamlLoadError> invalidConnectorCapability(
      std::string_view field_path, std::string message) {
    return std::unexpected(YamlLoadError{
        YamlLoadErrorCode::InvalidConnectorCapability,
        std::string(field_path) + " " + std::move(message)});
  }

  static std::string childPath(std::string_view base, std::string_view child) {
    return std::string(base) + "." + std::string(child);
  }

  static std::string childPath(std::string_view base, std::size_t index) {
    return std::string(base) + "[" + std::to_string(index) + "]";
  }

  static bool hasPrefix(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           text.substr(0, prefix.size()) == prefix;
  }
};

}  // namespace gt::datahub::runtime
