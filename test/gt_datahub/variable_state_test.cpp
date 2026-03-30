#include "gt_datahub/variable_state.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <variant>

namespace {

using gt::datahub::Quality;
using gt::datahub::Timestamp;
using gt::datahub::Value;
using gt::datahub::VariableState;

TEST(VariableStateTest, DefaultsMatchPublicContract) {
  const VariableState state{};

  EXPECT_TRUE(std::holds_alternative<std::monostate>(state.value));
  EXPECT_EQ(state.quality, Quality::Uncertain);
  EXPECT_FALSE(state.source_timestamp.has_value());
  EXPECT_FALSE(state.hub_timestamp.has_value());
  EXPECT_EQ(state.version, std::uint64_t{0});
  EXPECT_FALSE(state.initialized);
}

TEST(VariableStateTest, CanRepresentInitializedSnapshot) {
  VariableState state;
  state.value = Value{42.5};
  state.quality = Quality::Good;
  state.source_timestamp = Timestamp{std::chrono::milliseconds{100}};
  state.hub_timestamp = Timestamp{std::chrono::milliseconds{150}};
  state.version = 7;
  state.initialized = true;

  ASSERT_TRUE(std::holds_alternative<double>(state.value));
  EXPECT_EQ(std::get<double>(state.value), 42.5);
  EXPECT_EQ(state.quality, Quality::Good);
  ASSERT_TRUE(state.source_timestamp.has_value());
  EXPECT_EQ(*state.source_timestamp, Timestamp{std::chrono::milliseconds{100}});
  ASSERT_TRUE(state.hub_timestamp.has_value());
  EXPECT_EQ(*state.hub_timestamp, Timestamp{std::chrono::milliseconds{150}});
  EXPECT_EQ(state.version, std::uint64_t{7});
  EXPECT_TRUE(state.initialized);
}

}  // namespace
