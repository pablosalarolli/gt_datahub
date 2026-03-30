#include "runtime/yaml_loader.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace {

using gt::datahub::DataType;
using gt::datahub::Timestamp;
using gt::datahub::VariableRole;
using gt::datahub::runtime::LoadedDatahubConfig;
using gt::datahub::runtime::ProducerKind;
using gt::datahub::runtime::YamlLoadErrorCode;
using gt::datahub::runtime::YamlLoader;

TEST(YamlLoaderTest, MinimalValidYamlLoadsWithEmptyCollections) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables: []
)yaml");

  if (!config_result.has_value()) {
    FAIL() << static_cast<int>(config_result.error().code) << ": "
           << config_result.error().message;
  }
  const LoadedDatahubConfig& config = *config_result;

  EXPECT_EQ(config.schema_version, 1);
  EXPECT_TRUE(config.connectors.empty());
  EXPECT_TRUE(config.variables.empty());
}

TEST(YamlLoaderTest, DuplicateConnectorIdFails) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: opc_ua_main
      kind: opc_ua
    - id: opc_ua_main
      kind: opc_da
  variables: []
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code, YamlLoadErrorCode::DuplicateConnectorId);
  EXPECT_NE(config_result.error().message.find("duplicate connector id"),
            std::string::npos);
}

TEST(YamlLoaderTest, DuplicateVariableNameFails) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
    - name: TEMP
      data_type: Double
      role: Measurement
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code, YamlLoadErrorCode::DuplicateVariableName);
  EXPECT_NE(config_result.error().message.find("duplicate variable name"),
            std::string::npos);
}

TEST(YamlLoaderSmokeTest, SimpleConfigurationLoadsTypedConnectorsAndVariables) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: opc_ua_main
      kind: opc_ua
      enabled: true
      settings:
        server_url: "opc.tcp://127.0.0.1:49310"
        reconnect_ms: 2000
  variables:
    - name: CORRIDA_ATIVA
      data_type: Bool
      role: State
      default_value: false
      groups: [processo, corrida]
      labels: [online]
      historize: true
    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Float
      role: Measurement
      unit: "degC"
      precision: 1
      stale_after_ms: 5000
      min_value: 0.0
      max_value: 2000.0
    - name: EVENT_AT
      data_type: DateTime
      role: Other
      default_value: "2026-03-30T12:34:56.789Z"
)yaml");

  if (!config_result.has_value()) {
    FAIL() << static_cast<int>(config_result.error().code) << ": "
           << config_result.error().message;
  }
  const LoadedDatahubConfig& config = *config_result;

  ASSERT_EQ(config.connectors.size(), std::size_t{1});
  EXPECT_EQ(config.connectors[0].id, "opc_ua_main");
  EXPECT_EQ(config.connectors[0].kind, "opc_ua");
  EXPECT_TRUE(config.connectors[0].enabled);
  ASSERT_TRUE(config.connectors[0].settings.IsMap());
  EXPECT_EQ(config.connectors[0].settings["server_url"].as<std::string>(),
            "opc.tcp://127.0.0.1:49310");
  EXPECT_EQ(config.connectors[0].settings["reconnect_ms"].as<int>(), 2000);

  ASSERT_EQ(config.variables.size(), std::size_t{3});

  EXPECT_EQ(config.variables[0].name, "CORRIDA_ATIVA");
  EXPECT_EQ(config.variables[0].data_type, DataType::Bool);
  EXPECT_EQ(config.variables[0].role, VariableRole::State);
  EXPECT_EQ(config.variables[0].groups,
            (std::vector<std::string>{"processo", "corrida"}));
  EXPECT_EQ(config.variables[0].labels, (std::vector<std::string>{"online"}));
  ASSERT_TRUE(config.variables[0].default_value.has_value());
  ASSERT_TRUE(std::holds_alternative<bool>(*config.variables[0].default_value));
  EXPECT_FALSE(std::get<bool>(*config.variables[0].default_value));
  EXPECT_TRUE(config.variables[0].historize);

  EXPECT_EQ(config.variables[1].name, "TEMP_MAX_FUNDO_PANELA");
  EXPECT_EQ(config.variables[1].data_type, DataType::Float);
  EXPECT_EQ(config.variables[1].role, VariableRole::Measurement);
  EXPECT_EQ(config.variables[1].unit, "degC");
  ASSERT_TRUE(config.variables[1].precision.has_value());
  EXPECT_EQ(*config.variables[1].precision, 1);
  ASSERT_TRUE(config.variables[1].stale_after_ms.has_value());
  EXPECT_EQ(*config.variables[1].stale_after_ms, std::chrono::milliseconds{5000});
  ASSERT_TRUE(config.variables[1].min_value.has_value());
  ASSERT_TRUE(std::holds_alternative<float>(*config.variables[1].min_value));
  EXPECT_FLOAT_EQ(std::get<float>(*config.variables[1].min_value), 0.0f);
  ASSERT_TRUE(config.variables[1].max_value.has_value());
  ASSERT_TRUE(std::holds_alternative<float>(*config.variables[1].max_value));
  EXPECT_FLOAT_EQ(std::get<float>(*config.variables[1].max_value), 2000.0f);

  EXPECT_EQ(config.variables[2].name, "EVENT_AT");
  EXPECT_EQ(config.variables[2].data_type, DataType::DateTime);
  ASSERT_TRUE(config.variables[2].default_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<Timestamp>(*config.variables[2].default_value));
}

TEST(YamlLoaderTest, BindingReferencingUnknownConnectorFails) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
  producer_bindings:
    - id: pb_temp
      variable_name: TEMP
      producer_kind: connector
      connector_id: opc_ua_missing
      binding:
        type: opc_ua.node
        item_id: "TEMP"
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code, YamlLoadErrorCode::UnknownConnector);
  EXPECT_NE(config_result.error().message.find("unknown connector"),
            std::string::npos);
}

TEST(YamlLoaderTest, MoreThanOneActiveProducerForSameVariableFails) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
  producer_bindings:
    - id: pb_temp_1
      variable_name: TEMP
      producer_kind: internal
      enabled: true
    - id: pb_temp_2
      variable_name: TEMP
      producer_kind: internal
      enabled: true
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code,
            YamlLoadErrorCode::InvalidProducerOwnership);
  EXPECT_NE(config_result.error().message.find("more than one active producer"),
            std::string::npos);
}

TEST(YamlLoaderTest, ProducerKindInternalWithoutConnectorIdIsAccepted) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
  producer_bindings:
    - id: pb_temp_internal
      variable_name: TEMP
      producer_kind: internal
      enabled: true
)yaml");

  if (!config_result.has_value()) {
    FAIL() << static_cast<int>(config_result.error().code) << ": "
           << config_result.error().message;
  }
  const LoadedDatahubConfig& config = *config_result;

  ASSERT_EQ(config.producer_bindings.size(), std::size_t{1});
  EXPECT_EQ(config.producer_bindings[0].producer_kind, ProducerKind::Internal);
  EXPECT_FALSE(config.producer_bindings[0].connector_id.has_value());
  EXPECT_FALSE(config.producer_bindings[0].acquisition.has_value());
  EXPECT_FALSE(config.producer_bindings[0].binding.has_value());
}

TEST(YamlLoaderTest, ProducerKindInternalWithExternalFieldsFails) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: opc_ua_main
      kind: opc_ua
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
  producer_bindings:
    - id: pb_temp_internal
      variable_name: TEMP
      producer_kind: internal
      connector_id: opc_ua_main
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code, YamlLoadErrorCode::InvalidValue);
  EXPECT_NE(config_result.error().message.find("must be absent"),
            std::string::npos);
}

TEST(YamlLoaderTest, ConsumerBindingsWithInvalidModeFail) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: opc_ua_main
      kind: opc_ua
  variables:
    - name: ALARME
      data_type: Bool
      role: Alarm
  consumer_bindings:
    - id: cb_alarme
      variable_name: ALARME
      connector_id: opc_ua_main
      trigger:
        mode: manual
      binding:
        type: opc_ua.node
        item_id: "ALARME"
)yaml");

  ASSERT_FALSE(config_result.has_value());
  EXPECT_EQ(config_result.error().code,
            YamlLoadErrorCode::InvalidConnectorCapability);
  EXPECT_NE(config_result.error().message.find("not supported"),
            std::string::npos);
}

TEST(YamlLoaderTest, FileExportSourceColumnCompilesCanonicalSelector) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: file_main
      kind: file
  variables:
    - name: TEMP
      data_type: Float
      role: Measurement
  file_exports:
    - id: exp_temp
      connector_id: file_main
      format: csv
      target_template: "saida/${system.now}.csv"
      trigger:
        mode: manual
      columns:
        - name: temp
          source: hub.TEMP.value
)yaml");

  if (!config_result.has_value()) {
    FAIL() << static_cast<int>(config_result.error().code) << ": "
           << config_result.error().message;
  }
  const LoadedDatahubConfig& config = *config_result;

  ASSERT_EQ(config.file_exports.size(), std::size_t{1});
  ASSERT_EQ(config.file_exports[0].columns.size(), std::size_t{1});
  EXPECT_TRUE(config.file_exports[0].compiled_target_template.hasInterpolations());
  ASSERT_TRUE(config.file_exports[0].columns[0].compiled_source.has_value());
  EXPECT_FALSE(config.file_exports[0].columns[0].compiled_expression.has_value());
}

TEST(YamlLoaderSmokeTest, ConsolidatedSpecificationYamlValidates) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1

  connectors:
    - id: opc_ua_main
      kind: opc_ua
      enabled: true
      settings:
        server_url: "opc.tcp://127.0.0.1:49310"
        reconnect_ms: 2000
        request_timeout_ms: 1000

    - id: opc_da_main
      kind: opc_da
      enabled: true
      settings:
        server_name: "Kepware.KEPServerEX.V6"
        host: "192.168.0.50"
        reconnect_ms: 2000
        request_timeout_ms: 1000

    - id: file_main
      kind: file
      enabled: true
      settings:
        base_path: "C:/dados/processo"
        flush_ms: 1000

  variables:
    - name: NUMERO_CORRIDA
      data_type: UInt32
      role: State
      description: "Numero da corrida em andamento"

    - name: CORRIDA_ATIVA
      data_type: Bool
      role: State
      default_value: false

    - name: EM_MANUTENCAO
      data_type: Bool
      role: State
      default_value: false

    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Float
      role: Measurement
      unit: "degC"
      precision: 1
      stale_after_ms: 5000

    - name: RECEITA_ATUAL
      data_type: String
      role: State

    - name: ALARME_ESCORIA
      data_type: Bool
      role: Alarm
      default_value: false

  producer_bindings:
    - id: pb_numero_corrida_opcua
      variable_name: NUMERO_CORRIDA
      producer_kind: connector
      connector_id: opc_ua_main
      enabled: true
      acquisition:
        mode: subscription
      binding:
        type: opc_ua.node
        ns: 2
        item_id: "N1-ACI.CONV1.NUMERO_CORRIDA"

    - id: pb_corrida_ativa_opcda
      variable_name: CORRIDA_ATIVA
      producer_kind: connector
      connector_id: opc_da_main
      enabled: true
      acquisition:
        mode: polling
        poll_interval_ms: 500
      binding:
        type: opc_da.item
        item_id: "ACIARIA.CONV1.CORRIDA_ATIVA"
        access_path: ""

    - id: pb_receita_texto
      variable_name: RECEITA_ATUAL
      producer_kind: connector
      connector_id: file_main
      enabled: true
      acquisition:
        mode: polling
        poll_interval_ms: 1000
      binding:
        type: file.text
        path_template: "entrada/receita_${hub.NUMERO_CORRIDA.value}.txt"
        encoding: "utf-8"
        read_mode: whole_file

    - id: pb_temp_interna
      variable_name: TEMP_MAX_FUNDO_PANELA
      producer_kind: internal
      enabled: true

  consumer_bindings:
    - id: cb_alarme_escoria_opcua
      variable_name: ALARME_ESCORIA
      connector_id: opc_ua_main
      enabled: true
      trigger:
        mode: on_change
      binding:
        type: opc_ua.node
        ns: 2
        item_id: "N1-ACI.CONV1.ALARME_ESCORIA"

    - id: cb_alarme_escoria_opcda
      variable_name: ALARME_ESCORIA
      connector_id: opc_da_main
      enabled: true
      trigger:
        mode: on_change
      binding:
        type: opc_da.item
        item_id: "ACIARIA.CONV1.ALARME_ESCORIA"
        access_path: ""

  file_exports:
    - id: exp_corrida
      enabled: true
      connector_id: file_main
      format: csv
      target_template: "corridas/corrida_${hub.NUMERO_CORRIDA.value}.csv"
      append: true
      write_header_if_missing: true
      trigger:
        mode: periodic
        period_ms: 1000
      activation:
        run_while:
          all:
            - source: hub.CORRIDA_ATIVA.value
              op: eq
              value: true
            - not:
                source: hub.EM_MANUTENCAO.value
                op: eq
                value: true
        finalize_on_stop: true
      columns:
        - name: ts
          expression: "${export.captured_at}"
        - name: export_id
          expression: "${context.export_id}"
        - name: session_id
          expression: "${context.export_session_id}"
        - name: row_index
          expression: "${context.row_index}"
        - name: numero_corrida
          expression: "${hub.NUMERO_CORRIDA.value}"
        - name: receita
          expression: "${hub.RECEITA_ATUAL.value}"
        - name: temp_max
          expression: "${hub.TEMP_MAX_FUNDO_PANELA.value}"
        - name: quality
          expression: "${hub.TEMP_MAX_FUNDO_PANELA.quality}"

    - id: exp_snapshot_manual
      enabled: true
      connector_id: file_main
      format: csv
      target_template: "snapshots/snapshot_${system.now}.csv"
      append: false
      write_header_if_missing: true
      trigger:
        mode: manual
      columns:
        - name: ts
          expression: "${export.captured_at}"
        - name: trigger_mode
          expression: "${context.trigger_mode}"
        - name: numero_corrida
          expression: "${hub.NUMERO_CORRIDA.value}"
        - name: alarme
          expression: "${hub.ALARME_ESCORIA.value}"
)yaml");

  if (!config_result.has_value()) {
    FAIL() << static_cast<int>(config_result.error().code) << ": "
           << config_result.error().message;
  }
  const LoadedDatahubConfig& config = *config_result;

  EXPECT_EQ(config.connectors.size(), std::size_t{3});
  EXPECT_EQ(config.variables.size(), std::size_t{6});
  ASSERT_EQ(config.producer_bindings.size(), std::size_t{4});
  ASSERT_EQ(config.consumer_bindings.size(), std::size_t{2});
  ASSERT_EQ(config.file_exports.size(), std::size_t{2});
  EXPECT_EQ(config.producer_bindings[3].producer_kind, ProducerKind::Internal);
  ASSERT_TRUE(config.producer_bindings[2].binding.has_value());
  ASSERT_TRUE(
      config.producer_bindings[2].binding->compiled_path_template.has_value());
  ASSERT_EQ(config.file_exports[0].columns.size(), std::size_t{8});
  EXPECT_TRUE(config.file_exports[0].activation.IsMap());
  EXPECT_TRUE(config.file_exports[0].compiled_target_template.hasInterpolations());
  ASSERT_NE(config.file_exports[0].compiled_activation, nullptr);
  ASSERT_TRUE(config.file_exports[0].columns[0].compiled_expression.has_value());
  EXPECT_EQ(config.file_exports[0].trigger.mode, "periodic");
  ASSERT_TRUE(config.file_exports[0].trigger.period_ms.has_value());
  EXPECT_EQ(*config.file_exports[0].trigger.period_ms,
            std::chrono::milliseconds{1000});
  EXPECT_EQ(config.file_exports[1].trigger.mode, "manual");
  EXPECT_FALSE(config.file_exports[1].trigger.period_ms.has_value());
}

}  // namespace
