#include "core/predicate_compiler.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace {

using gt::datahub::Value;
using gt::datahub::core::ConditionAll;
using gt::datahub::core::ConditionAny;
using gt::datahub::core::ConditionLeaf;
using gt::datahub::core::ConditionNot;
using gt::datahub::core::PredicateCompiler;
using gt::datahub::core::PredicateOperator;
using gt::datahub::core::SelectorContext;

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

}  // namespace
