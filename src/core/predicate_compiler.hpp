#pragma once

#include "core/selector_parser.hpp"
#include "gt_datahub/value.hpp"

#include <yaml-cpp/yaml.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gt::datahub::core {

/**
 * Predicate operators supported by the baseline activation model.
 */
enum class PredicateOperator {
  Eq,
  Ne,
  Gt,
  Ge,
  Lt,
  Le,
  In,
  NotIn,
  IsTrue,
  IsFalse,
  IsNull,
  IsNotNull
};

/**
 * Error returned when an activation predicate cannot be compiled.
 */
struct PredicateCompileError {
  std::string message;
};

using PredicateValue = std::variant<Value, std::vector<Value>>;

struct ConditionNode;

struct ConditionLeaf {
  CompiledSelector source;
  PredicateOperator op;
  std::optional<PredicateValue> value;
};

struct ConditionAll {
  std::vector<std::unique_ptr<ConditionNode>> children;
};

struct ConditionAny {
  std::vector<std::unique_ptr<ConditionNode>> children;
};

struct ConditionNot {
  std::unique_ptr<ConditionNode> child;
};

/**
 * Immutable AST node for one compiled activation predicate.
 */
struct ConditionNode {
  using Variant =
      std::variant<ConditionLeaf, ConditionAll, ConditionAny, ConditionNot>;

  Variant value;

  ConditionNode() = delete;
  explicit ConditionNode(ConditionLeaf leaf) : value(std::move(leaf)) {}
  explicit ConditionNode(ConditionAll all) : value(std::move(all)) {}
  explicit ConditionNode(ConditionAny any) : value(std::move(any)) {}
  explicit ConditionNode(ConditionNot not_node) : value(std::move(not_node)) {}

  ConditionNode(const ConditionNode&) = delete;
  ConditionNode& operator=(const ConditionNode&) = delete;
  ConditionNode(ConditionNode&&) noexcept = default;
  ConditionNode& operator=(ConditionNode&&) noexcept = default;
  ~ConditionNode() = default;
};

/**
 * Compiler for `activation.run_while`.
 */
class PredicateCompiler {
 public:
  /**
   * Compiles one activation predicate tree using the provided selector context.
   */
  static std::expected<std::unique_ptr<ConditionNode>, PredicateCompileError>
  compile(const YAML::Node& node, SelectorContext selector_context) {
    if (!node || !node.IsMap()) {
      return invalid("predicate node must be a YAML mapping");
    }

    const YAML::Node source = node["source"];
    const YAML::Node all = node["all"];
    const YAML::Node any = node["any"];
    const YAML::Node not_node = node["not"];

    const int shape_count = static_cast<int>(static_cast<bool>(source)) +
                            static_cast<int>(static_cast<bool>(all)) +
                            static_cast<int>(static_cast<bool>(any)) +
                            static_cast<int>(static_cast<bool>(not_node));
    if (shape_count != 1) {
      return invalid(
          "predicate node must declare exactly one of `source`, `all`, `any` or `not`");
    }

    if (source) {
      return compileLeaf(node, selector_context);
    }
    if (all) {
      return compileComposite(all, selector_context, CompositeKind::All);
    }
    if (any) {
      return compileComposite(any, selector_context, CompositeKind::Any);
    }

    return compileNot(not_node, selector_context);
  }

 private:
  enum class CompositeKind { All, Any };

  static std::expected<std::unique_ptr<ConditionNode>, PredicateCompileError>
  compileLeaf(const YAML::Node& node, SelectorContext selector_context) {
    const auto source_text = requireScalar(node["source"], "`source`");
    if (!source_text.has_value()) {
      return std::unexpected(source_text.error());
    }

    const auto selector =
        SelectorParser::parseCanonical(*source_text, selector_context);
    if (!selector.has_value()) {
      return invalid(selector.error().message);
    }

    const auto op_text = requireScalar(node["op"], "`op`");
    if (!op_text.has_value()) {
      return std::unexpected(op_text.error());
    }

    const auto op = parseOperator(*op_text);
    if (!op.has_value()) {
      return std::unexpected(op.error());
    }

    const bool expects_value = isBinaryOperator(*op);
    const YAML::Node value_node = node["value"];
    if (expects_value && !value_node) {
      return invalid("predicate operator requires a `value` field");
    }
    if (!expects_value && value_node) {
      return invalid("predicate unary operator must not declare `value`");
    }

    ConditionLeaf leaf{std::move(*selector), *op, std::nullopt};
    if (expects_value) {
      const auto value = parsePredicateValue(value_node, *op);
      if (!value.has_value()) {
        return std::unexpected(value.error());
      }
      leaf.value = std::move(*value);
    }

    return std::make_unique<ConditionNode>(std::move(leaf));
  }

  static std::expected<std::unique_ptr<ConditionNode>, PredicateCompileError>
  compileComposite(const YAML::Node& node, SelectorContext selector_context,
                   CompositeKind kind) {
    if (!node.IsSequence()) {
      return invalid("composite predicate node must be a YAML sequence");
    }
    if (node.size() == 0) {
      return invalid("composite predicate sequence must not be empty");
    }

    std::vector<std::unique_ptr<ConditionNode>> children;
    children.reserve(node.size());

    for (std::size_t i = 0; i < node.size(); ++i) {
      auto child = compile(node[i], selector_context);
      if (!child.has_value()) {
        return std::unexpected(child.error());
      }
      children.push_back(std::move(*child));
    }

    if (kind == CompositeKind::All) {
      return std::make_unique<ConditionNode>(
          ConditionAll{std::move(children)});
    }

    return std::make_unique<ConditionNode>(ConditionAny{std::move(children)});
  }

  static std::expected<std::unique_ptr<ConditionNode>, PredicateCompileError>
  compileNot(const YAML::Node& node, SelectorContext selector_context) {
    auto child = compile(node, selector_context);
    if (!child.has_value()) {
      return std::unexpected(child.error());
    }

    return std::make_unique<ConditionNode>(
        ConditionNot{std::move(*child)});
  }

  static std::expected<PredicateOperator, PredicateCompileError> parseOperator(
      std::string_view text) {
    if (text == "eq") {
      return PredicateOperator::Eq;
    }
    if (text == "ne") {
      return PredicateOperator::Ne;
    }
    if (text == "gt") {
      return PredicateOperator::Gt;
    }
    if (text == "ge") {
      return PredicateOperator::Ge;
    }
    if (text == "lt") {
      return PredicateOperator::Lt;
    }
    if (text == "le") {
      return PredicateOperator::Le;
    }
    if (text == "in") {
      return PredicateOperator::In;
    }
    if (text == "not_in") {
      return PredicateOperator::NotIn;
    }
    if (text == "is_true") {
      return PredicateOperator::IsTrue;
    }
    if (text == "is_false") {
      return PredicateOperator::IsFalse;
    }
    if (text == "is_null") {
      return PredicateOperator::IsNull;
    }
    if (text == "is_not_null") {
      return PredicateOperator::IsNotNull;
    }

    return invalid("unsupported predicate operator: " + std::string(text));
  }

  static bool isBinaryOperator(PredicateOperator op) noexcept {
    switch (op) {
      case PredicateOperator::Eq:
      case PredicateOperator::Ne:
      case PredicateOperator::Gt:
      case PredicateOperator::Ge:
      case PredicateOperator::Lt:
      case PredicateOperator::Le:
      case PredicateOperator::In:
      case PredicateOperator::NotIn:
        return true;
      case PredicateOperator::IsTrue:
      case PredicateOperator::IsFalse:
      case PredicateOperator::IsNull:
      case PredicateOperator::IsNotNull:
        return false;
    }

    return false;
  }

  static std::expected<PredicateValue, PredicateCompileError> parsePredicateValue(
      const YAML::Node& node, PredicateOperator op) {
    if (op == PredicateOperator::In || op == PredicateOperator::NotIn) {
      if (!node.IsSequence()) {
        return invalid("predicate operator requires a YAML sequence value");
      }

      std::vector<Value> values;
      values.reserve(node.size());
      for (std::size_t i = 0; i < node.size(); ++i) {
        const auto value = parseScalarValue(node[i]);
        if (!value.has_value()) {
          return std::unexpected(value.error());
        }
        values.push_back(std::move(*value));
      }

      return PredicateValue{std::move(values)};
    }

    const auto scalar = parseScalarValue(node);
    if (!scalar.has_value()) {
      return std::unexpected(scalar.error());
    }

    return PredicateValue{std::move(*scalar)};
  }

  static std::expected<Value, PredicateCompileError> parseScalarValue(
      const YAML::Node& node) {
    if (node.IsNull()) {
      return Value{std::monostate{}};
    }
    if (!node.IsScalar()) {
      return invalid("predicate value must be a scalar or a sequence of scalars");
    }

    const std::string scalar = node.Scalar();
    if (scalar == "true") {
      return Value{true};
    }
    if (scalar == "false") {
      return Value{false};
    }

    if (looksLikeInteger(scalar)) {
      if (!scalar.empty() && scalar.front() == '-') {
        return Value{static_cast<std::int64_t>(std::stoll(scalar))};
      }
      return Value{static_cast<std::uint64_t>(std::stoull(scalar))};
    }

    if (looksLikeFloatingPoint(scalar)) {
      return Value{std::stod(scalar)};
    }

    return Value{scalar};
  }

  static std::expected<std::string, PredicateCompileError> requireScalar(
      const YAML::Node& node, std::string_view field_name) {
    if (!node) {
      return invalid("predicate field " + std::string(field_name) +
                     " is required");
    }
    if (!node.IsScalar()) {
      return invalid("predicate field " + std::string(field_name) +
                     " must be a scalar");
    }

    return node.as<std::string>();
  }

  static bool looksLikeInteger(std::string_view text) noexcept {
    if (text.empty()) {
      return false;
    }

    std::size_t index = 0;
    if (text.front() == '-') {
      if (text.size() == 1) {
        return false;
      }
      index = 1;
    }

    for (; index < text.size(); ++index) {
      if (text[index] < '0' || text[index] > '9') {
        return false;
      }
    }

    return true;
  }

  static bool looksLikeFloatingPoint(std::string_view text) noexcept {
    bool has_decimal = false;
    bool has_exponent = false;
    std::size_t index = 0;

    if (text.empty()) {
      return false;
    }
    if (text.front() == '-') {
      if (text.size() == 1) {
        return false;
      }
      index = 1;
    }

    bool has_digit = false;
    for (; index < text.size(); ++index) {
      const char ch = text[index];
      if (ch >= '0' && ch <= '9') {
        has_digit = true;
        continue;
      }
      if (ch == '.' && !has_decimal && !has_exponent) {
        has_decimal = true;
        continue;
      }
      if ((ch == 'e' || ch == 'E') && !has_exponent && has_digit &&
          index + 1 < text.size()) {
        has_exponent = true;
        if (text[index + 1] == '+' || text[index + 1] == '-') {
          ++index;
        }
        continue;
      }
      return false;
    }

    return has_digit && (has_decimal || has_exponent);
  }

  static std::unexpected<PredicateCompileError> invalid(std::string message) {
    return std::unexpected(PredicateCompileError{std::move(message)});
  }
};

}  // namespace gt::datahub::core
