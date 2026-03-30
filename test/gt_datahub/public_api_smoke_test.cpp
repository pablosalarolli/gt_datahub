#include "gt_datahub/i_datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using gt::datahub::DataType;
using gt::datahub::IDataHub;
using gt::datahub::IDataHubRuntime;
using gt::datahub::IInternalProducer;
using gt::datahub::OpenProducerError;
using gt::datahub::Quality;
using gt::datahub::ResolveError;
using gt::datahub::RuntimeError;
using gt::datahub::SubmitError;
using gt::datahub::TriggerError;
using gt::datahub::UpdateRequest;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::VariableState;

class SmokeHub final : public IDataHub {
 public:
  std::optional<VariableState> getState(
      std::string_view variable_name) const override {
    if (variable_name != "READY") {
      return std::nullopt;
    }

    VariableState state;
    state.value = Value{true};
    state.quality = Quality::Good;
    state.initialized = true;
    return state;
  }

  std::optional<VariableDefinition> getDefinition(
      std::string_view variable_name) const override {
    if (variable_name != "READY") {
      return std::nullopt;
    }

    VariableDefinition definition;
    definition.name = "READY";
    definition.data_type = DataType::Bool;
    definition.role = VariableRole::State;
    definition.default_value = Value{false};
    return definition;
  }

  std::vector<std::string> listVariables() const override { return {"READY"}; }

  std::expected<std::string, ResolveError> resolveText(
      std::string_view expression) const override {
    return std::string{expression};
  }
};

class SmokeProducer final : public IInternalProducer {
 public:
  std::string_view bindingId() const noexcept override { return "ready_writer"; }

  std::string_view variableName() const noexcept override { return "READY"; }

  std::expected<void, SubmitError> submit(UpdateRequest) override { return {}; }
};

class SmokeRuntime final : public IDataHubRuntime {
 public:
  std::expected<void, RuntimeError> start() override { return {}; }

  void stop() override {}

  IDataHub& hub() noexcept override { return m_hub; }

  const IDataHub& hub() const noexcept override { return m_hub; }

  std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
  openInternalProducer(std::string_view) override {
    return std::make_unique<SmokeProducer>();
  }

  std::expected<void, TriggerError> triggerFileExport(std::string_view) override {
    return {};
  }

 private:
  SmokeHub m_hub;
};

TEST(PublicApiSmokeTest, MinimalAppCompilesAgainstThePublicApi) {
  SmokeRuntime runtime;

  ASSERT_TRUE(runtime.start().has_value());
  auto names = runtime.hub().listVariables();
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names.front(), "READY");

  auto state = runtime.hub().getState("READY");
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(std::get<bool>(state->value));

  auto producer = runtime.openInternalProducer("ready_writer");
  ASSERT_TRUE(producer.has_value());
  EXPECT_TRUE((*producer)->submit(UpdateRequest{Value{true}}).has_value());
  EXPECT_TRUE(runtime.triggerFileExport("manual_ready").has_value());
}

}  // namespace
