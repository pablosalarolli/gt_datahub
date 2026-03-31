#include "core/text_resolver.hpp"
#include "core/text_template_compiler.hpp"

#include "gtest/gtest.h"

#include <cstdint>
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
using gt::datahub::core::TextTemplateCompiler;
using gt::datahub::core::TextResolveContext;
using gt::datahub::core::TextResolver;

class StubResolveHub final : public IDataHub {
 public:
  std::optional<VariableState> getState(
      std::string_view variable_name) const override {
    if (variable_name == "INPUT_FILE") {
      VariableState state;
      state.value = Value{};
      state.quality = Quality::Uncertain;
      state.initialized = false;
      return state;
    }

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
    if (variable_name == "INPUT_FILE") {
      VariableDefinition definition;
      definition.name = "INPUT_FILE";
      definition.data_type = DataType::String;
      definition.role = VariableRole::Other;
      return definition;
    }

    if (variable_name != "READY") {
      return std::nullopt;
    }

    VariableDefinition definition;
    definition.name = "READY";
    definition.data_type = DataType::Bool;
    definition.role = VariableRole::State;
    return definition;
  }

  std::vector<std::string> listVariables() const override {
    return {"READY", "INPUT_FILE"};
  }

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

TEST(TextResolverTest,
     PathTemplateMissingValueReturnsNulloptWhileTargetTemplateInterpolatesEmpty) {
  StubResolveHub hub;

  const auto path_template = TextTemplateCompiler::compile(
      "entrada/${hub.INPUT_FILE.value}.txt", SelectorContext::FilePathTemplate);
  ASSERT_TRUE(path_template.has_value());

  const auto target_template = TextTemplateCompiler::compile(
      "saida/${hub.INPUT_FILE.value}.txt", SelectorContext::FileExport);
  ASSERT_TRUE(target_template.has_value());

  const auto resolved_path =
      TextResolver::resolvePathTemplate(*path_template, hub, {});
  ASSERT_TRUE(resolved_path.has_value());
  EXPECT_FALSE(resolved_path->has_value());

  const auto resolved_target =
      TextResolver::resolveTargetTemplate(*target_template, hub, {});
  ASSERT_TRUE(resolved_target.has_value());
  EXPECT_EQ(*resolved_target, "saida/.txt");
}

}  // namespace
