#include "gt_datahub/i_datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using gt::datahub::IDataHub;
using gt::datahub::IDataHubRuntime;
using gt::datahub::IInternalProducer;
using gt::datahub::OpenProducerError;
using gt::datahub::OpenProducerErrorCode;
using gt::datahub::Quality;
using gt::datahub::ResolveError;
using gt::datahub::RuntimeError;
using gt::datahub::SubmitError;
using gt::datahub::Timestamp;
using gt::datahub::TriggerError;
using gt::datahub::UpdateRequest;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::VariableState;
using gt::datahub::DataType;

class StubDataHub final : public IDataHub {
 public:
  std::optional<VariableState> getState(
      std::string_view variable_name) const override {
    if (variable_name != "TEMP_MAX_FUNDO_PANELA") {
      return std::nullopt;
    }

    VariableState state;
    state.value = Value{1350.5};
    state.quality = Quality::Good;
    state.version = 3;
    state.initialized = true;
    return state;
  }

  std::optional<VariableDefinition> getDefinition(
      std::string_view variable_name) const override {
    if (variable_name != "TEMP_MAX_FUNDO_PANELA") {
      return std::nullopt;
    }

    VariableDefinition definition;
    definition.name = "TEMP_MAX_FUNDO_PANELA";
    definition.data_type = DataType::Double;
    definition.role = VariableRole::Measurement;
    definition.unit = "degC";
    return definition;
  }

  std::vector<std::string> listVariables() const override {
    return {"TEMP_MAX_FUNDO_PANELA"};
  }

  std::expected<std::string, ResolveError> resolveText(
      std::string_view expression) const override {
    return std::string{expression};
  }
};

class StubInternalProducer final : public IInternalProducer {
 public:
  explicit StubInternalProducer(std::string binding_id, std::string variable_name)
      : m_binding_id(std::move(binding_id)),
        m_variable_name(std::move(variable_name)) {}

  std::string_view bindingId() const noexcept override { return m_binding_id; }

  std::string_view variableName() const noexcept override {
    return m_variable_name;
  }

  std::expected<void, SubmitError> submit(UpdateRequest req) override {
    m_last_request = std::move(req);
    return {};
  }

  const std::optional<UpdateRequest>& lastRequest() const noexcept {
    return m_last_request;
  }

 private:
  std::string m_binding_id;
  std::string m_variable_name;
  std::optional<UpdateRequest> m_last_request;
};

class StubRuntime final : public IDataHubRuntime {
 public:
  std::expected<void, gt::datahub::RuntimeError> start() override {
    m_started = true;
    return {};
  }

  void stop() override { m_started = false; }

  IDataHub& hub() noexcept override { return m_hub; }

  const IDataHub& hub() const noexcept override { return m_hub; }

  std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
  openInternalProducer(std::string_view binding_id) override {
    if (binding_id != "internal_temp_writer") {
      return std::unexpected(OpenProducerError{
          OpenProducerErrorCode::UnknownBinding, "unknown binding"});
    }

    return std::make_unique<StubInternalProducer>(
        "internal_temp_writer", "TEMP_MAX_FUNDO_PANELA");
  }

  std::expected<void, TriggerError> triggerFileExport(
      std::string_view export_id) override {
    m_last_export_id = std::string{export_id};
    return {};
  }

  bool started() const noexcept { return m_started; }

  std::string_view lastExportId() const noexcept { return m_last_export_id; }

 private:
  StubDataHub m_hub;
  bool m_started{false};
  std::string m_last_export_id;
};

TEST(InterfaceMocksTest, SimpleStubsComposeAgainstInterfaces) {
  StubRuntime runtime;

  ASSERT_TRUE(runtime.start().has_value());
  EXPECT_TRUE(runtime.started());

  auto definition = runtime.hub().getDefinition("TEMP_MAX_FUNDO_PANELA");
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->name, "TEMP_MAX_FUNDO_PANELA");

  auto producer = runtime.openInternalProducer("internal_temp_writer");
  ASSERT_TRUE(producer.has_value());
  EXPECT_EQ((*producer)->bindingId(), "internal_temp_writer");
  EXPECT_EQ((*producer)->variableName(), "TEMP_MAX_FUNDO_PANELA");

  UpdateRequest request;
  request.value = Value{1351.75};
  request.quality = Quality::Good;
  request.source_timestamp = Timestamp{};
  EXPECT_TRUE((*producer)->submit(std::move(request)).has_value());

  EXPECT_TRUE(runtime.triggerFileExport("export_temp").has_value());
  EXPECT_EQ(runtime.lastExportId(), "export_temp");

  runtime.stop();
  EXPECT_FALSE(runtime.started());
}

TEST(InterfaceMocksTest, OpenInternalProducerPropagatesExpectedErrorType) {
  StubRuntime runtime;

  auto producer = runtime.openInternalProducer("missing_binding");

  ASSERT_FALSE(producer.has_value());
  EXPECT_EQ(producer.error().code, OpenProducerErrorCode::UnknownBinding);
}

}  // namespace
