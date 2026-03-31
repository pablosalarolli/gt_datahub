#include "runtime/datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <memory>
#include <regex>
#include <string_view>
#include <utility>

namespace {

using gt::datahub::ResolveErrorCode;
using gt::datahub::runtime::DataHubRuntime;

constexpr std::string_view kResolveTextYaml = R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: READY
      data_type: Bool
      role: State
      default_value: true
    - name: TEMP
      data_type: Double
      role: Measurement
  producer_bindings: []
  consumer_bindings: []
  file_exports: []
)yaml";

std::unique_ptr<DataHubRuntime> makeRuntime(std::string_view yaml_text) {
  auto runtime_result = DataHubRuntime::createFromString(yaml_text);
  if (!runtime_result.has_value()) {
    ADD_FAILURE() << runtime_result.error().message;
    return nullptr;
  }

  return std::move(*runtime_result);
}

TEST(ResolveTextRuntimeTest, PublicResolveSerializesKnownHubFieldsCanonically) {
  auto runtime = makeRuntime(kResolveTextYaml);
  ASSERT_NE(runtime, nullptr);

  const auto value = runtime->hub().resolveText("hub.READY.value");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "true");

  const auto quality = runtime->hub().resolveText("hub.READY.quality");
  ASSERT_TRUE(quality.has_value());
  EXPECT_EQ(*quality, "uncertain");

  const auto initialized = runtime->hub().resolveText("hub.READY.initialized");
  ASSERT_TRUE(initialized.has_value());
  EXPECT_EQ(*initialized, "true");

  const auto version = runtime->hub().resolveText("hub.READY.version");
  ASSERT_TRUE(version.has_value());
  EXPECT_EQ(*version, "0");
}

TEST(ResolveTextRuntimeTest, PublicResolveRejectsContextNamespace) {
  auto runtime = makeRuntime(kResolveTextYaml);
  ASSERT_NE(runtime, nullptr);

  const auto resolved = runtime->hub().resolveText("context.row_index");

  ASSERT_FALSE(resolved.has_value());
  EXPECT_EQ(resolved.error().code, ResolveErrorCode::InvalidNamespace);
}

TEST(ResolveTextRuntimeTest, PublicResolveReturnsEmptyStringForUninitializedValue) {
  auto runtime = makeRuntime(kResolveTextYaml);
  ASSERT_NE(runtime, nullptr);

  const auto resolved = runtime->hub().resolveText("hub.TEMP.value");

  ASSERT_TRUE(resolved.has_value());
  EXPECT_TRUE(resolved->empty());
}

TEST(ResolveTextRuntimeTest, PublicResolveResolvesCompleteTemplate) {
  auto runtime = makeRuntime(kResolveTextYaml);
  ASSERT_NE(runtime, nullptr);

  const auto resolved = runtime->hub().resolveText(
      "Ready ${hub.READY.value} @ ${export.captured_at} / ${system.now}");

  ASSERT_TRUE(resolved.has_value());

  static const std::regex kPattern(
      R"(^Ready true @ \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z / \d{8}T\d{9}$)");
  EXPECT_TRUE(std::regex_match(*resolved, kPattern)) << *resolved;
}

}  // namespace
