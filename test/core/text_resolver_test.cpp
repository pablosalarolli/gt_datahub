#include "core/text_resolver.hpp"

#include "gtest/gtest.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using gt::datahub::DataType;
using gt::datahub::IDataHub;
using gt::datahub::Quality;
using gt::datahub::ResolveError;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableRole;
using gt::datahub::VariableState;
using gt::datahub::core::SelectorContext;
using gt::datahub::core::TextResolveContext;
using gt::datahub::core::TextResolver;

class StubResolveHub final : public IDataHub {
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
    return definition;
  }

  std::vector<std::string> listVariables() const override { return {"READY"}; }

  std::expected<std::string, ResolveError> resolveText(
      std::string_view expression) const override {
    return std::string{expression};
  }
};

TEST(TextResolverTest, ContextRowIndexSerializesAsUnsignedDecimal) {
  StubResolveHub hub;

  TextResolveContext context;
  context.row_index = 42;

  const auto resolved = TextResolver::resolveExpression(
      "context.row_index", hub, SelectorContext::FileExport, context);

  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(*resolved, "42");
}

}  // namespace
