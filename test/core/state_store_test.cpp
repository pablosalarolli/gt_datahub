#include "core/state_store.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

using gt::datahub::DataType;
using gt::datahub::Quality;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::core::CompiledCatalog;
using gt::datahub::core::StateStore;

VariableDefinition makeDefinition(std::string name, DataType data_type) {
  VariableDefinition definition;
  definition.name = std::move(name);
  definition.data_type = data_type;
  definition.role = VariableRole::Other;
  return definition;
}

TEST(StateStoreTest, BootstrapCreatesOneEntryPerVariable) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("READY", DataType::Bool),
       makeDefinition("TEMP", DataType::Double)});

  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  EXPECT_EQ(store.size(), std::size_t{2});
  EXPECT_NE(store.findEntryByName("READY"), nullptr);
  EXPECT_NE(store.findEntryByName("TEMP"), nullptr);
}

TEST(StateStoreTest, DefaultValueInitializesStateAsInitializedTrue) {
  auto definition = makeDefinition("READY", DataType::Bool);
  definition.default_value = Value{false};

  auto catalog_result = CompiledCatalog::build({std::move(definition)});
  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  const auto state = store.getState("READY");
  ASSERT_TRUE(state.has_value());
  ASSERT_TRUE(std::holds_alternative<bool>(state->value));
  EXPECT_FALSE(std::get<bool>(state->value));
  EXPECT_EQ(state->quality, Quality::Uncertain);
  EXPECT_TRUE(state->initialized);
  EXPECT_EQ(state->version, std::uint64_t{0});
  EXPECT_FALSE(state->source_timestamp.has_value());
  EXPECT_FALSE(state->hub_timestamp.has_value());

  const auto* entry = store.findEntryByName("READY");
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->last_update_steady.has_value());
}

TEST(StateStoreTest, VariableWithoutDefaultStartsUninitialized) {
  auto catalog_result =
      CompiledCatalog::build({makeDefinition("COUNT", DataType::UInt32)});

  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  const auto state = store.getState("COUNT");
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(std::holds_alternative<std::monostate>(state->value));
  EXPECT_EQ(state->quality, Quality::Uncertain);
  EXPECT_FALSE(state->initialized);
  EXPECT_EQ(state->version, std::uint64_t{0});
}

TEST(StateStoreTest, GetStateReturnsNulloptForUnknownVariable) {
  auto catalog_result =
      CompiledCatalog::build({makeDefinition("READY", DataType::Bool)});

  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  EXPECT_FALSE(store.getState("UNKNOWN").has_value());
}

TEST(StateStoreTest, GetDefinitionReturnsPublicDefinitionForKnownVariable) {
  auto definition = makeDefinition("TEMP", DataType::Double);
  definition.unit = "degC";
  definition.stale_after_ms = std::chrono::milliseconds{5000};

  auto catalog_result = CompiledCatalog::build({std::move(definition)});
  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  const auto resolved_definition = store.getDefinition("TEMP");
  ASSERT_TRUE(resolved_definition.has_value());
  EXPECT_EQ(resolved_definition->name, "TEMP");
  EXPECT_EQ(resolved_definition->data_type, DataType::Double);
  EXPECT_EQ(resolved_definition->unit, "degC");
  ASSERT_TRUE(resolved_definition->stale_after_ms.has_value());
  EXPECT_EQ(*resolved_definition->stale_after_ms, std::chrono::milliseconds(5000));
}

TEST(StateStoreTest, ListVariablesReturnsStableSetAfterBootstrap) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("A", DataType::Bool), makeDefinition("B", DataType::String)});

  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  const auto first = store.listVariables();
  const auto second = store.listVariables();

  EXPECT_EQ(first, (std::vector<std::string>{"A", "B"}));
  EXPECT_EQ(second, first);
}

TEST(StateStoreSmokeTest, BootstrapWithTwoVariablesSupportsReads) {
  auto ready = makeDefinition("READY", DataType::Bool);
  ready.default_value = Value{true};

  auto temperature = makeDefinition("TEMP", DataType::Double);

  auto catalog_result =
      CompiledCatalog::build({std::move(ready), std::move(temperature)});
  ASSERT_TRUE(catalog_result.has_value());
  const StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  ASSERT_EQ(store.size(), std::size_t{2});
  const auto ready_state = store.getState("READY");
  ASSERT_TRUE(ready_state.has_value());
  EXPECT_TRUE(ready_state->initialized);
  EXPECT_TRUE(std::get<bool>(ready_state->value));

  const auto temp_state = store.getState("TEMP");
  ASSERT_TRUE(temp_state.has_value());
  EXPECT_FALSE(temp_state->initialized);

  const auto names = store.listVariables();
  EXPECT_EQ(names, (std::vector<std::string>{"READY", "TEMP"}));
}

}  // namespace
