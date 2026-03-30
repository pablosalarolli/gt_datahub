#include "core/compiled_catalog.hpp"

#include "gtest/gtest.h"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using gt::datahub::DataType;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::core::CompiledCatalog;
using gt::datahub::core::CompiledVariableDefinition;
using gt::datahub::core::TransparentStringHash;

VariableDefinition makeDefinition(std::string name, DataType data_type) {
  VariableDefinition definition;
  definition.name = std::move(name);
  definition.data_type = data_type;
  definition.role = VariableRole::Other;
  return definition;
}

struct StringViewProbe {
  std::string_view value;

  operator std::string_view() const noexcept { return value; }
};

bool operator==(const StringViewProbe& lhs, const std::string& rhs) noexcept {
  return lhs.value == rhs;
}

bool operator==(const std::string& lhs, const StringViewProbe& rhs) noexcept {
  return lhs == rhs.value;
}

static_assert(std::is_same_v<TransparentStringHash::is_transparent, void>);

TEST(CompiledCatalogTest, LookupExistingNameReturnsCompiledDefinition) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("READY", DataType::Bool),
       makeDefinition("TEMP_MAX_FUNDO_PANELA", DataType::Double)});

  ASSERT_TRUE(catalog_result.has_value());
  const CompiledCatalog& catalog = *catalog_result;

  const CompiledVariableDefinition* definition =
      catalog.findByName("TEMP_MAX_FUNDO_PANELA");

  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->public_definition.name, "TEMP_MAX_FUNDO_PANELA");
  EXPECT_EQ(definition->public_definition.data_type, DataType::Double);
  EXPECT_EQ(definition->variable_index, std::size_t{1});
}

TEST(CompiledCatalogTest, LookupMissingNameReturnsCleanFailure) {
  auto catalog_result =
      CompiledCatalog::build({makeDefinition("READY", DataType::Bool)});

  ASSERT_TRUE(catalog_result.has_value());
  const CompiledCatalog& catalog = *catalog_result;

  EXPECT_EQ(catalog.findByName("UNKNOWN"), nullptr);
  EXPECT_FALSE(catalog.findIndexByName("UNKNOWN").has_value());
}

TEST(CompiledCatalogTest, NameIndexesRemainStableAfterBuild) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("A", DataType::Bool), makeDefinition("B", DataType::Int32),
       makeDefinition("C", DataType::String)});

  ASSERT_TRUE(catalog_result.has_value());
  const CompiledCatalog& catalog = *catalog_result;

  ASSERT_EQ(catalog.size(), std::size_t{3});
  EXPECT_EQ(catalog.findIndexByName("A"), std::optional<std::size_t>{0});
  EXPECT_EQ(catalog.findIndexByName("B"), std::optional<std::size_t>{1});
  EXPECT_EQ(catalog.findIndexByName("C"), std::optional<std::size_t>{2});
  ASSERT_NE(catalog.findByIndex(0), nullptr);
  EXPECT_EQ(catalog.findByIndex(0)->variable_index, std::size_t{0});
  ASSERT_NE(catalog.findByIndex(1), nullptr);
  EXPECT_EQ(catalog.findByIndex(1)->variable_index, std::size_t{1});
  ASSERT_NE(catalog.findByIndex(2), nullptr);
  EXPECT_EQ(catalog.findByIndex(2)->variable_index, std::size_t{2});
}

TEST(CompiledCatalogTest,
     HeterogeneousLookupWorksWithoutRequiringTemporaryStringKey) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("TEMP_MAX_FUNDO_PANELA", DataType::Double)});

  ASSERT_TRUE(catalog_result.has_value());
  const CompiledCatalog& catalog = *catalog_result;
  const StringViewProbe probe{"TEMP_MAX_FUNDO_PANELA"};

  const auto it = catalog.nameIndex().find(probe);

  ASSERT_NE(it, catalog.nameIndex().end());
  EXPECT_EQ(it->second, std::size_t{0});
}

TEST(CompiledCatalogTest, MinimalCatalogCanCompileAndQueryDefinitions) {
  auto catalog_result =
      CompiledCatalog::build({makeDefinition("READY", DataType::Bool)});

  ASSERT_TRUE(catalog_result.has_value());
  const CompiledCatalog& catalog = *catalog_result;

  EXPECT_EQ(catalog.size(), std::size_t{1});
  ASSERT_EQ(catalog.definitions().size(), std::size_t{1});
  EXPECT_EQ(catalog.definitions().front().public_definition.name, "READY");
  EXPECT_EQ(catalog.definitions().front().public_definition.data_type,
            DataType::Bool);
}

TEST(CompiledCatalogTest, DuplicateVariableNamesFailBuild) {
  auto catalog_result = CompiledCatalog::build(
      {makeDefinition("READY", DataType::Bool),
       makeDefinition("READY", DataType::Double)});

  ASSERT_FALSE(catalog_result.has_value());
  EXPECT_FALSE(catalog_result.error().message.empty());
}

}  // namespace
