#include "core/selector_parser.hpp"

#include "gtest/gtest.h"

#include <optional>
#include <string_view>
#include <variant>

namespace {

using gt::datahub::core::CompiledSelector;
using gt::datahub::core::HubField;
using gt::datahub::core::HubSelector;
using gt::datahub::core::SelectorContext;
using gt::datahub::core::SelectorParser;

TEST(SelectorParserTest, CanonicalPureSelectorCompiles) {
  const auto selector_result = SelectorParser::parseCanonical(
      "hub.TEMP_MAX_FUNDO_PANELA.quality", SelectorContext::FileExport);

  ASSERT_TRUE(selector_result.has_value());
  const CompiledSelector& selector = *selector_result;

  ASSERT_TRUE(selector.hubVariableName().has_value());
  EXPECT_EQ(*selector.hubVariableName(), "TEMP_MAX_FUNDO_PANELA");

  const auto* hub_selector = std::get_if<HubSelector>(&selector.value);
  ASSERT_NE(hub_selector, nullptr);
  EXPECT_EQ(hub_selector->field, HubField::Quality);
}

}  // namespace
