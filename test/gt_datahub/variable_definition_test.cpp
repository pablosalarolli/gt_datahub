#include "gt_datahub/variable_definition.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <variant>

namespace {

using gt::datahub::DataType;
using gt::datahub::Timestamp;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;

TEST(VariableDefinitionTest, DefaultsAndOptionalsAreCoherent) {
  VariableDefinition definition;
  definition.data_type = DataType::Bool;

  EXPECT_TRUE(definition.name.empty());
  EXPECT_EQ(definition.role, VariableRole::Other);
  EXPECT_TRUE(definition.description.empty());
  EXPECT_TRUE(definition.unit.empty());
  EXPECT_TRUE(definition.groups.empty());
  EXPECT_TRUE(definition.labels.empty());
  EXPECT_FALSE(definition.default_value.has_value());
  EXPECT_FALSE(definition.min_value.has_value());
  EXPECT_FALSE(definition.max_value.has_value());
  EXPECT_FALSE(definition.precision.has_value());
  EXPECT_FALSE(definition.historize);
  EXPECT_FALSE(definition.stale_after_ms.has_value());
}

TEST(VariableDefinitionTest, DefaultValueStoresCompatibleBooleanValue) {
  VariableDefinition definition;
  definition.name = "MOTOR_ENABLED";
  definition.data_type = DataType::Bool;
  definition.default_value = Value{true};

  ASSERT_TRUE(definition.default_value.has_value());
  ASSERT_TRUE(std::holds_alternative<bool>(*definition.default_value));
  EXPECT_TRUE(std::get<bool>(*definition.default_value));
}

TEST(VariableDefinitionTest, BoundsAndDatetimeDefaultsCanBeStored) {
  VariableDefinition definition;
  definition.name = "PROCESS_START";
  definition.data_type = DataType::DateTime;
  definition.default_value = Value{Timestamp{std::chrono::seconds{10}}};
  definition.min_value = Value{Timestamp{std::chrono::seconds{1}}};
  definition.max_value = Value{Timestamp{std::chrono::seconds{20}}};
  definition.precision = 3;
  definition.historize = true;
  definition.stale_after_ms = std::chrono::milliseconds{5000};

  ASSERT_TRUE(definition.default_value.has_value());
  EXPECT_TRUE(std::holds_alternative<Timestamp>(*definition.default_value));
  ASSERT_TRUE(definition.min_value.has_value());
  EXPECT_TRUE(std::holds_alternative<Timestamp>(*definition.min_value));
  ASSERT_TRUE(definition.max_value.has_value());
  EXPECT_TRUE(std::holds_alternative<Timestamp>(*definition.max_value));
  ASSERT_TRUE(definition.precision.has_value());
  EXPECT_EQ(*definition.precision, 3);
  EXPECT_TRUE(definition.historize);
  ASSERT_TRUE(definition.stale_after_ms.has_value());
  EXPECT_EQ(*definition.stale_after_ms, std::chrono::milliseconds(5000));
}

}  // namespace
