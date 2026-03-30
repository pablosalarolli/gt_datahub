#include "runtime/datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using gt::datahub::OpenProducerErrorCode;
using gt::datahub::Quality;
using gt::datahub::RuntimeErrorCode;
using gt::datahub::SubmitErrorCode;
using gt::datahub::UpdateRequest;
using gt::datahub::runtime::DataHubRuntime;
using gt::datahub::runtime::OnChangeNotification;

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

constexpr std::string_view kInternalProducerYaml = R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Double
      role: Measurement
  producer_bindings:
    - id: pb_temp_interna
      variable_name: TEMP_MAX_FUNDO_PANELA
      producer_kind: internal
  consumer_bindings: []
  file_exports: []
)yaml";

constexpr std::string_view kOnChangeYaml = R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: opc_ua_main
      kind: opc_ua
  variables:
    - name: ALARME_ESCORIA
      data_type: Bool
      role: Alarm
  producer_bindings:
    - id: pb_alarme_interno
      variable_name: ALARME_ESCORIA
      producer_kind: internal
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
  file_exports: []
)yaml";

struct FakeSinkProbe {
  void accept(const OnChangeNotification& notification) {
    ++received_count;
    versions.push_back(notification.version);
  }

  std::size_t received_count{0};
  std::vector<std::uint64_t> versions;
};

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

TEST(DataHubRuntimeTest, OpenInternalProducerAcceptsValidInternalBinding) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);
  EXPECT_EQ((*producer_result)->bindingId(), "pb_temp_interna");
  EXPECT_EQ((*producer_result)->variableName(), "TEMP_MAX_FUNDO_PANELA");
}

TEST(DataHubRuntimeTest, OpeningSameBindingTwiceFailsWithAlreadyOpen) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto first_open = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(first_open.has_value());

  auto second_open = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_FALSE(second_open.has_value());
  EXPECT_EQ(second_open.error().code, OpenProducerErrorCode::AlreadyOpen);
}

TEST(DataHubRuntimeTest, DestroyingHandleAllowsReopen) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto first_open = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(first_open.has_value());
  first_open->reset();

  auto second_open = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(second_open.has_value());
  ASSERT_NE(*second_open, nullptr);
}

TEST(DataHubRuntimeTest, SubmitRejectsIncompatibleType) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest request;
  request.value = std::string("tipo_invalido");

  const auto submit_result = (*producer_result)->submit(std::move(request));
  ASSERT_FALSE(submit_result.has_value());
  EXPECT_EQ(submit_result.error().code, SubmitErrorCode::InvalidType);
}

TEST(DataHubRuntimeTest, AppPublishesThroughInternalProducer) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest request;
  request.value = 87.5;
  request.quality = Quality::Good;

  ASSERT_TRUE((*producer_result)->submit(std::move(request)).has_value());

  auto state = runtime->hub().getState("TEMP_MAX_FUNDO_PANELA");
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->initialized);
  EXPECT_EQ(state->quality, Quality::Good);
  EXPECT_EQ(state->version, std::uint64_t{1});
  ASSERT_TRUE(std::holds_alternative<double>(state->value));
  EXPECT_DOUBLE_EQ(std::get<double>(state->value), 87.5);
}

TEST(DataHubRuntimeTest,
     AcceptedUpdateIncrementsVersionAndGeneratesInternalNotification) {
  auto runtime = makeRuntime(kOnChangeYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_alarme_interno");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest request;
  request.value = true;

  ASSERT_TRUE((*producer_result)->submit(std::move(request)).has_value());

  auto state = runtime->hub().getState("ALARME_ESCORIA");
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->version, std::uint64_t{1});
  EXPECT_EQ(runtime->pendingOnChangeNotificationCount(
                "cb_alarme_escoria_opcua"),
            std::size_t{1});
  EXPECT_EQ(runtime->totalPendingOnChangeNotificationCount(), std::size_t{1});
}

TEST(DataHubRuntimeTest, VariableWithoutConsumerBindingDoesNotGenerateOnChangeWork) {
  auto runtime = makeRuntime(kInternalProducerYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_temp_interna");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest request;
  request.value = 90.0;

  ASSERT_TRUE((*producer_result)->submit(std::move(request)).has_value());
  EXPECT_EQ(runtime->totalPendingOnChangeNotificationCount(), std::size_t{0});
}

TEST(DataHubRuntimeTest, OnChangeQueuesEveryAcceptedUpdateEvenWhenValueIsEqual) {
  auto runtime = makeRuntime(kOnChangeYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_alarme_interno");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest first_request;
  first_request.value = true;
  ASSERT_TRUE((*producer_result)->submit(std::move(first_request)).has_value());

  UpdateRequest second_request;
  second_request.value = true;
  ASSERT_TRUE((*producer_result)->submit(std::move(second_request)).has_value());

  auto state = runtime->hub().getState("ALARME_ESCORIA");
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->version, std::uint64_t{2});

  const auto notifications = runtime->drainOnChangeNotificationsForTesting(
      "cb_alarme_escoria_opcua");
  ASSERT_EQ(notifications.size(), std::size_t{2});
  EXPECT_EQ(notifications[0].version, std::uint64_t{1});
  EXPECT_EQ(notifications[1].version, std::uint64_t{2});
  EXPECT_EQ(runtime->totalPendingOnChangeNotificationCount(), std::size_t{0});
}

TEST(DataHubRuntimeTest, InternalUpdateTriggersFakeSinkAfterDispatchDrain) {
  auto runtime = makeRuntime(kOnChangeYaml);
  ASSERT_NE(runtime, nullptr);
  ASSERT_TRUE(runtime->start().has_value());

  auto producer_result = runtime->openInternalProducer("pb_alarme_interno");
  ASSERT_TRUE(producer_result.has_value());
  ASSERT_NE(*producer_result, nullptr);

  UpdateRequest request;
  request.value = true;
  ASSERT_TRUE((*producer_result)->submit(std::move(request)).has_value());

  FakeSinkProbe fake_sink;
  for (const auto& notification :
       runtime->drainOnChangeNotificationsForTesting(
           "cb_alarme_escoria_opcua")) {
    fake_sink.accept(notification);
  }

  EXPECT_EQ(fake_sink.received_count, std::size_t{1});
  ASSERT_EQ(fake_sink.versions.size(), std::size_t{1});
  EXPECT_EQ(fake_sink.versions.front(), std::uint64_t{1});
}

}  // namespace
