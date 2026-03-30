#include "runtime/datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <variant>

namespace {

using gt::datahub::RuntimeErrorCode;
using gt::datahub::runtime::DataHubRuntime;

constexpr std::string_view kRuntimeCoreYaml = R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: file_main
      kind: file
  variables:
    - name: READY
      data_type: Bool
      role: State
      default_value: false
  file_exports:
    - id: exp_ready
      connector_id: file_main
      format: csv
      target_template: "saida/${system.now}.csv"
      trigger:
        mode: periodic
        period_ms: 1000
      columns:
        - name: ready
          source: hub.READY.value
)yaml";

std::unique_ptr<DataHubRuntime> makeRuntime(std::string_view yaml_text) {
  auto runtime_result = DataHubRuntime::createFromString(yaml_text);
  if (!runtime_result.has_value()) {
    ADD_FAILURE() << runtime_result.error().message;
    return nullptr;
  }

  return std::move(*runtime_result);
}

TEST(DataHubRuntimeTest, StartTwiceFailsWithAlreadyStarted) {
  auto runtime = makeRuntime(kRuntimeCoreYaml);
  ASSERT_NE(runtime, nullptr);

  ASSERT_TRUE(runtime->start().has_value());

  const auto second_start = runtime->start();
  ASSERT_FALSE(second_start.has_value());
  EXPECT_EQ(second_start.error().code, RuntimeErrorCode::AlreadyStarted);
}

TEST(DataHubRuntimeTest, StopIsIdempotent) {
  auto runtime = makeRuntime(kRuntimeCoreYaml);
  ASSERT_NE(runtime, nullptr);

  ASSERT_TRUE(runtime->start().has_value());
  runtime->stop();
  EXPECT_FALSE(runtime->started());
  EXPECT_EQ(runtime->activeConnectorRuntimeCount(), std::size_t{0});
  EXPECT_FALSE(runtime->schedulerActive());

  runtime->stop();
  EXPECT_FALSE(runtime->started());
  EXPECT_EQ(runtime->activeConnectorRuntimeCount(), std::size_t{0});
  EXPECT_FALSE(runtime->schedulerActive());
}

TEST(DataHubRuntimeTest,
     BootstrapStaysColdUntilStartAndRuntimeRisesWithoutRealAdapters) {
  auto runtime = makeRuntime(kRuntimeCoreYaml);
  ASSERT_NE(runtime, nullptr);

  EXPECT_FALSE(runtime->started());
  EXPECT_EQ(runtime->activeConnectorRuntimeCount(), std::size_t{0});
  EXPECT_FALSE(runtime->schedulerActive());

  auto names = runtime->hub().listVariables();
  ASSERT_EQ(names.size(), std::size_t{1});
  EXPECT_EQ(names.front(), "READY");

  auto definition = runtime->hub().getDefinition("READY");
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->name, "READY");

  auto state = runtime->hub().getState("READY");
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->initialized);
  ASSERT_TRUE(std::holds_alternative<bool>(state->value));
  EXPECT_FALSE(std::get<bool>(state->value));

  ASSERT_TRUE(runtime->start().has_value());
  EXPECT_TRUE(runtime->started());
  EXPECT_EQ(runtime->activeConnectorRuntimeCount(), std::size_t{1});
  EXPECT_TRUE(runtime->schedulerActive());

  runtime->stop();
  EXPECT_FALSE(runtime->started());
  EXPECT_EQ(runtime->activeConnectorRuntimeCount(), std::size_t{0});
  EXPECT_FALSE(runtime->schedulerActive());
}

}  // namespace
