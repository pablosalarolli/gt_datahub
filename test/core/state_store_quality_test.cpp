#include "core/state_store.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <variant>

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

TEST(StateStoreQualityTest, VariableWithoutStaleConfigurationDoesNotAge) {
  auto catalog_result =
      CompiledCatalog::build({makeDefinition("READY", DataType::Bool)});

  ASSERT_TRUE(catalog_result.has_value());
  StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  auto* entry = store.findEntryByName("READY");
  ASSERT_NE(entry, nullptr);
  entry->value = Value{true};
  entry->raw_quality = Quality::Good;
  entry->initialized = true;
  entry->last_update_steady = std::chrono::steady_clock::time_point{};

  const auto state = store.getStateAt(
      "READY", std::chrono::steady_clock::time_point{} + std::chrono::hours{24});

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->quality, Quality::Good);
}

TEST(StateStoreQualityTest, StaleIsDerivedOnReadNotBySweepThread) {
  auto definition = makeDefinition("TEMP", DataType::Double);
  definition.stale_after_ms = std::chrono::milliseconds{100};

  auto catalog_result = CompiledCatalog::build({std::move(definition)});
  ASSERT_TRUE(catalog_result.has_value());
  StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  auto* entry = store.findEntryByName("TEMP");
  ASSERT_NE(entry, nullptr);
  entry->value = Value{42.5};
  entry->raw_quality = Quality::Good;
  entry->initialized = true;
  entry->last_update_steady = std::chrono::steady_clock::time_point{};

  const auto state = store.getStateAt(
      "TEMP", std::chrono::steady_clock::time_point{} + std::chrono::milliseconds{250});

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->quality, Quality::Stale);
  EXPECT_EQ(entry->raw_quality, Quality::Good);
}

TEST(StateStoreQualityTest, BadPrevailsOverStale) {
  auto definition = makeDefinition("TEMP", DataType::Double);
  definition.stale_after_ms = std::chrono::milliseconds{100};

  auto catalog_result = CompiledCatalog::build({std::move(definition)});
  ASSERT_TRUE(catalog_result.has_value());
  StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  auto* entry = store.findEntryByName("TEMP");
  ASSERT_NE(entry, nullptr);
  entry->value = Value{42.5};
  entry->raw_quality = Quality::Bad;
  entry->initialized = true;
  entry->last_update_steady = std::chrono::steady_clock::time_point{};

  const auto state = store.getStateAt(
      "TEMP", std::chrono::steady_clock::time_point{} + std::chrono::milliseconds{250});

  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->quality, Quality::Bad);
}

TEST(StateStoreQualitySmokeTest, EffectiveQualityReflectsLazyStaleSemantics) {
  auto definition = makeDefinition("READY", DataType::Bool);
  definition.default_value = Value{true};
  definition.stale_after_ms = std::chrono::milliseconds{100};

  auto catalog_result = CompiledCatalog::build({std::move(definition)});
  ASSERT_TRUE(catalog_result.has_value());
  StateStore store = StateStore::bootstrap(std::move(*catalog_result));

  const auto* entry = store.findEntryByName("READY");
  ASSERT_NE(entry, nullptr);
  ASSERT_TRUE(entry->last_update_steady.has_value());

  const auto fresh_state =
      store.getStateAt("READY", *entry->last_update_steady + std::chrono::milliseconds{50});
  ASSERT_TRUE(fresh_state.has_value());
  EXPECT_EQ(fresh_state->quality, Quality::Uncertain);
  EXPECT_TRUE(fresh_state->initialized);
  EXPECT_TRUE(std::get<bool>(fresh_state->value));

  const auto stale_state =
      store.getStateAt("READY", *entry->last_update_steady + std::chrono::milliseconds{150});
  ASSERT_TRUE(stale_state.has_value());
  EXPECT_EQ(stale_state->quality, Quality::Stale);
}

}  // namespace
