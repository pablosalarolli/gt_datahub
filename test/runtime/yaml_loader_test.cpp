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
using gt::datahub::runtime::YamlLoadErrorCode;
using gt::datahub::runtime::YamlLoader;

TEST(YamlLoaderTest, MinimalValidYamlLoadsWithEmptyCollections) {
  const auto config_result = YamlLoader::loadFromString(R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables: []
)yaml");

  ASSERT_TRUE(config_result.has_value());
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

  ASSERT_TRUE(config_result.has_value());
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

}  // namespace
