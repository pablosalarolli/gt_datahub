#include "gt_datahub/update_request.hpp"
#include "gt_datahub/variable_definition.hpp"
#include "gt_datahub/variable_state.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <variant>

namespace {

using gt::datahub::DataType;
using gt::datahub::Quality;
using gt::datahub::Timestamp;
using gt::datahub::UpdateRequest;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::VariableState;

TEST(PublicModelSmokeTest, DefinitionStateAndRequestComposeTogether) {
  VariableDefinition definition;
  definition.name = "TEMP_MAX_FUNDO_PANELA";
  definition.data_type = DataType::Double;
  definition.role = VariableRole::Measurement;
  definition.unit = "degC";
  definition.default_value = Value{1350.5};
  definition.precision = 1;
  definition.stale_after_ms = std::chrono::milliseconds{5000};

  VariableState state;
  state.value = *definition.default_value;
  state.quality = Quality::Uncertain;
  state.version = 1;
  state.initialized = true;

  UpdateRequest request;
  request.value = Value{1361.25};
  request.quality = Quality::Good;
  request.source_timestamp = Timestamp{std::chrono::milliseconds{250}};

  EXPECT_EQ(definition.name, "TEMP_MAX_FUNDO_PANELA");
  ASSERT_TRUE(std::holds_alternative<double>(*definition.default_value));
  EXPECT_EQ(std::get<double>(*definition.default_value), 1350.5);
  ASSERT_TRUE(std::holds_alternative<double>(state.value));
  EXPECT_EQ(std::get<double>(state.value), 1350.5);
  ASSERT_TRUE(std::holds_alternative<double>(request.value));
  EXPECT_EQ(std::get<double>(request.value), 1361.25);
  EXPECT_EQ(request.quality, Quality::Good);
}

}  // namespace
