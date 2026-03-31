#include "core/predicate_compiler.hpp"

#include "gtest/gtest.h"

#include <cstdint>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace {

using gt::datahub::IDataHub;
using gt::datahub::ResolveError;
using gt::datahub::Value;
using gt::datahub::VariableDefinition;
using gt::datahub::VariableState;
using gt::datahub::core::ConditionAll;
using gt::datahub::core::ConditionAny;
using gt::datahub::core::ConditionLeaf;
using gt::datahub::core::ConditionNot;
using gt::datahub::core::EvalContext;
using gt::datahub::core::PredicateCompiler;
using gt::datahub::core::PredicateOperator;
using gt::datahub::core::SelectorContext;

class StubEvalHub final : public IDataHub {
 public:
  StubEvalHub() {
    corrida_ativa.value = true;
    corrida_ativa.initialized = true;

    alarme_escoria.value = false;
    alarme_escoria.initialized = true;

    nivel_log.value = std::int32_t{3};
    nivel_log.initialized = true;

    forcar_salvamento.value = false;
    forcar_salvamento.initialized = true;

    em_manutencao.value = false;
    em_manutencao.initialized = true;
  }

  std::optional<VariableState> getState(
      std::string_view variable_name) const override {
    if (variable_name == "CORRIDA_ATIVA") {
      return corrida_ativa;
    }
    if (variable_name == "ALARME_ESCORIA") {
      return alarme_escoria;
    }
    if (variable_name == "NIVEL_LOG") {
      return nivel_log;
    }
    if (variable_name == "FORCAR_SALVAMENTO") {
      return forcar_salvamento;
    }
    if (variable_name == "EM_MANUTENCAO") {
      return em_manutencao;
    }
    if (variable_name == "OPCIONAL") {
      return opcional;
    }

    return std::nullopt;
  }

  std::optional<VariableDefinition> getDefinition(
      std::string_view variable_name) const override {
    VariableDefinition definition;
    definition.name = std::string(variable_name);
    return definition;
  }

  std::vector<std::string> listVariables() const override { return {}; }

  std::expected<std::string, ResolveError> resolveText(
      std::string_view) const override {
    return std::string{};
  }

  VariableState corrida_ativa;
  VariableState alarme_escoria;
  VariableState nivel_log;
  VariableState forcar_salvamento;
  VariableState em_manutencao;
  VariableState opcional;
};

TEST(PredicateCompilerTest, UnaryAndBinaryOperatorsValidateValuePresence) {
  const auto valid_unary = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.ALARME_ESCORIA.value
op: is_false
)yaml"),
      SelectorContext::FileExport);

  ASSERT_TRUE(valid_unary.has_value());
  const auto* unary_leaf = std::get_if<ConditionLeaf>(&(*valid_unary)->value);
  ASSERT_NE(unary_leaf, nullptr);
  EXPECT_EQ(unary_leaf->op, PredicateOperator::IsFalse);
  EXPECT_FALSE(unary_leaf->value.has_value());

  const auto binary_missing_value = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.CORRIDA_ATIVA.value
op: eq
)yaml"),
      SelectorContext::FileExport);

  ASSERT_FALSE(binary_missing_value.has_value());
  EXPECT_NE(binary_missing_value.error().message.find("requires a `value` field"),
            std::string::npos);

  const auto unary_with_value = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.ALARME_ESCORIA.value
op: is_false
value: false
)yaml"),
      SelectorContext::FileExport);

  ASSERT_FALSE(unary_with_value.has_value());
  EXPECT_NE(unary_with_value.error().message.find("must not declare `value`"),
            std::string::npos);

  const auto valid_binary = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.NIVEL_LOG.value
op: ge
value: 2
)yaml"),
      SelectorContext::FileExport);

  ASSERT_TRUE(valid_binary.has_value());
  const auto* leaf = std::get_if<ConditionLeaf>(&(*valid_binary)->value);
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(leaf->op, PredicateOperator::Ge);
  ASSERT_TRUE(leaf->value.has_value());
  ASSERT_TRUE(std::holds_alternative<Value>(*leaf->value));
}

TEST(PredicateCompilerTest, ActivationAstCompilesWithAllAnyAndNot) {
  const auto predicate_result = PredicateCompiler::compile(
      YAML::Load(R"yaml(
all:
  - source: hub.CORRIDA_ATIVA.value
    op: eq
    value: true
  - any:
      - source: hub.FORCAR_SALVAMENTO.value
        op: eq
        value: true
      - not:
          source: hub.EM_MANUTENCAO.value
          op: eq
          value: true
)yaml"),
      SelectorContext::FileExport);

  ASSERT_TRUE(predicate_result.has_value());
  const auto* root = std::get_if<ConditionAll>(&(*predicate_result)->value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->children.size(), std::size_t{2});

  const auto* first_leaf = std::get_if<ConditionLeaf>(&root->children[0]->value);
  ASSERT_NE(first_leaf, nullptr);
  EXPECT_EQ(first_leaf->op, PredicateOperator::Eq);

  const auto* any_node = std::get_if<ConditionAny>(&root->children[1]->value);
  ASSERT_NE(any_node, nullptr);
  ASSERT_EQ(any_node->children.size(), std::size_t{2});
  ASSERT_NE(std::get_if<ConditionLeaf>(&any_node->children[0]->value), nullptr);

  const auto* not_node = std::get_if<ConditionNot>(&any_node->children[1]->value);
  ASSERT_NE(not_node, nullptr);
  ASSERT_NE(not_node->child, nullptr);
  ASSERT_NE(std::get_if<ConditionLeaf>(&not_node->child->value), nullptr);
}

TEST(PredicateCompilerTest, BinaryLeafEvaluatesAgainstTypedHubState) {
  const auto predicate_result = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.NIVEL_LOG.value
op: ge
value: 2
)yaml"),
      SelectorContext::FileExport);

  ASSERT_TRUE(predicate_result.has_value());

  StubEvalHub hub;
  const EvalContext context{hub};
  EXPECT_TRUE((*predicate_result)->evaluate(context));

  hub.nivel_log.value = std::int32_t{1};
  EXPECT_FALSE((*predicate_result)->evaluate(context));
}

TEST(PredicateCompilerTest, UnaryLeafEvaluatesFalseAndNullPredicates) {
  const auto false_predicate = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.ALARME_ESCORIA.value
op: is_false
)yaml"),
      SelectorContext::FileExport);
  ASSERT_TRUE(false_predicate.has_value());

  const auto null_predicate = PredicateCompiler::compile(
      YAML::Load(R"yaml(
source: hub.OPCIONAL.value
op: is_null
)yaml"),
      SelectorContext::FileExport);
  ASSERT_TRUE(null_predicate.has_value());

  StubEvalHub hub;
  const EvalContext context{hub};
  EXPECT_TRUE((*false_predicate)->evaluate(context));
  EXPECT_TRUE((*null_predicate)->evaluate(context));

  hub.alarme_escoria.value = true;
  hub.opcional.value = std::string("preenchido");
  hub.opcional.initialized = true;
  EXPECT_FALSE((*false_predicate)->evaluate(context));
  EXPECT_FALSE((*null_predicate)->evaluate(context));
}

TEST(PredicateCompilerTest, CompositeAllAnyAndNotEvaluatesConstTree) {
  const auto predicate_result = PredicateCompiler::compile(
      YAML::Load(R"yaml(
all:
  - source: hub.CORRIDA_ATIVA.value
    op: eq
    value: true
  - any:
      - source: hub.FORCAR_SALVAMENTO.value
        op: eq
        value: true
      - not:
          source: hub.EM_MANUTENCAO.value
          op: eq
          value: true
)yaml"),
      SelectorContext::FileExport);

  ASSERT_TRUE(predicate_result.has_value());

  StubEvalHub hub;
  const EvalContext context{hub};
  EXPECT_TRUE((*predicate_result)->evaluate(context));

  hub.em_manutencao.value = true;
  EXPECT_FALSE((*predicate_result)->evaluate(context));

  hub.forcar_salvamento.value = true;
  EXPECT_TRUE((*predicate_result)->evaluate(context));
}

}  // namespace
