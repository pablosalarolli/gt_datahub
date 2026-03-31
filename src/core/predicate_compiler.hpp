#pragma once

#include "core/selector_parser.hpp"
#include "gt_datahub/i_datahub.hpp"
#include "gt_datahub/value.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
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

/**
 * Runtime context consumed when a compiled activation AST is evaluated.
 *
 * The selector surface matches the file-export execution context, but the
 * values remain strongly typed instead of going through textual serialization.
 */
struct EvalContext {
  const IDataHub& hub;
  std::optional<std::string> export_id;
  std::optional<std::uint64_t> export_session_id;
  std::optional<std::uint64_t> row_index;
  std::optional<std::string> trigger_mode;
  std::optional<std::string> target_path;
  std::optional<Timestamp> session_started_at;
  std::optional<Timestamp> export_captured_at;
  std::optional<Timestamp> system_now;
};

struct ConditionNode;

struct ConditionLeaf {
  CompiledSelector source;
  PredicateOperator op;
  std::optional<PredicateValue> value;

  bool evaluate(const EvalContext& ctx) const;
};

struct ConditionAll {
  std::vector<std::unique_ptr<ConditionNode>> children;

  bool evaluate(const EvalContext& ctx) const;
};

struct ConditionAny {
  std::vector<std::unique_ptr<ConditionNode>> children;

  bool evaluate(const EvalContext& ctx) const;
};

struct ConditionNot {
  std::unique_ptr<ConditionNode> child;

  bool evaluate(const EvalContext& ctx) const;
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

  bool evaluate(const EvalContext& ctx) const;
};

namespace predicate_detail {

struct NumericValue {
  enum class Kind { Signed, Unsigned, Floating };

  Kind kind{Kind::Signed};
  std::int64_t signed_value{0};
  std::uint64_t unsigned_value{0};
  long double floating_value{0.0L};
};

inline Value nullValue() { return Value{std::monostate{}}; }

inline std::string serializeQuality(Quality quality) {
  switch (quality) {
    case Quality::Good:
      return "good";
    case Quality::Bad:
      return "bad";
    case Quality::Stale:
      return "stale";
    case Quality::Uncertain:
      return "uncertain";
  }

  return "uncertain";
}

inline std::optional<NumericValue> asNumeric(const Value& value) {
  if (const auto* signed_32 = std::get_if<std::int32_t>(&value)) {
    return NumericValue{NumericValue::Kind::Signed, *signed_32, 0, 0.0L};
  }
  if (const auto* unsigned_32 = std::get_if<std::uint32_t>(&value)) {
    return NumericValue{NumericValue::Kind::Unsigned, 0, *unsigned_32, 0.0L};
  }
  if (const auto* signed_64 = std::get_if<std::int64_t>(&value)) {
    return NumericValue{NumericValue::Kind::Signed, *signed_64, 0, 0.0L};
  }
  if (const auto* unsigned_64 = std::get_if<std::uint64_t>(&value)) {
    return NumericValue{NumericValue::Kind::Unsigned, 0, *unsigned_64, 0.0L};
  }
  if (const auto* float_value = std::get_if<float>(&value)) {
    return NumericValue{NumericValue::Kind::Floating, 0, 0,
                        static_cast<long double>(*float_value)};
  }
  if (const auto* double_value = std::get_if<double>(&value)) {
    return NumericValue{NumericValue::Kind::Floating, 0, 0,
                        static_cast<long double>(*double_value)};
  }

  return std::nullopt;
}

inline int compareNumeric(const NumericValue& lhs,
                          const NumericValue& rhs) noexcept {
  if (lhs.kind == NumericValue::Kind::Floating ||
      rhs.kind == NumericValue::Kind::Floating) {
    const long double lhs_value =
        lhs.kind == NumericValue::Kind::Floating
            ? lhs.floating_value
            : (lhs.kind == NumericValue::Kind::Signed
                   ? static_cast<long double>(lhs.signed_value)
                   : static_cast<long double>(lhs.unsigned_value));
    const long double rhs_value =
        rhs.kind == NumericValue::Kind::Floating
            ? rhs.floating_value
            : (rhs.kind == NumericValue::Kind::Signed
                   ? static_cast<long double>(rhs.signed_value)
                   : static_cast<long double>(rhs.unsigned_value));

    if (lhs_value < rhs_value) {
      return -1;
    }
    if (lhs_value > rhs_value) {
      return 1;
    }
    return 0;
  }

  if (lhs.kind == NumericValue::Kind::Signed &&
      rhs.kind == NumericValue::Kind::Signed) {
    if (lhs.signed_value < rhs.signed_value) {
      return -1;
    }
    if (lhs.signed_value > rhs.signed_value) {
      return 1;
    }
    return 0;
  }

  if (lhs.kind == NumericValue::Kind::Unsigned &&
      rhs.kind == NumericValue::Kind::Unsigned) {
    if (lhs.unsigned_value < rhs.unsigned_value) {
      return -1;
    }
    if (lhs.unsigned_value > rhs.unsigned_value) {
      return 1;
    }
    return 0;
  }

  if (lhs.kind == NumericValue::Kind::Signed) {
    if (lhs.signed_value < 0) {
      return -1;
    }

    const auto lhs_unsigned = static_cast<std::uint64_t>(lhs.signed_value);
    if (lhs_unsigned < rhs.unsigned_value) {
      return -1;
    }
    if (lhs_unsigned > rhs.unsigned_value) {
      return 1;
    }
    return 0;
  }

  if (rhs.signed_value < 0) {
    return 1;
  }

  const auto rhs_unsigned = static_cast<std::uint64_t>(rhs.signed_value);
  if (lhs.unsigned_value < rhs_unsigned) {
    return -1;
  }
  if (lhs.unsigned_value > rhs_unsigned) {
    return 1;
  }
  return 0;
}

inline bool isNull(const Value& value) noexcept {
  return std::holds_alternative<std::monostate>(value);
}

inline Value resolveHubSelectorValue(const HubSelector& selector,
                                     const EvalContext& ctx) {
  const auto state = ctx.hub.getState(selector.variable_name);
  if (!state.has_value()) {
    return nullValue();
  }

  switch (selector.field) {
    case HubField::Value:
      if (!state->initialized || std::holds_alternative<std::monostate>(state->value)) {
        return nullValue();
      }
      return state->value;
    case HubField::Quality:
      return Value{serializeQuality(state->quality)};
    case HubField::SourceTimestamp:
      return state->source_timestamp.has_value() ? Value{*state->source_timestamp}
                                                 : nullValue();
    case HubField::HubTimestamp:
      return state->hub_timestamp.has_value() ? Value{*state->hub_timestamp}
                                              : nullValue();
    case HubField::Version:
      return Value{state->version};
    case HubField::Initialized:
      return Value{state->initialized};
  }

  return nullValue();
}

inline Value resolveContextSelectorValue(const ContextSelector& selector,
                                         const EvalContext& ctx) {
  switch (selector.field) {
    case ContextField::ExportId:
      return ctx.export_id.has_value() ? Value{*ctx.export_id} : nullValue();
    case ContextField::ExportSessionId:
      return ctx.export_session_id.has_value() ? Value{*ctx.export_session_id}
                                               : nullValue();
    case ContextField::RowIndex:
      return ctx.row_index.has_value() ? Value{*ctx.row_index} : nullValue();
    case ContextField::TriggerMode:
      return ctx.trigger_mode.has_value() ? Value{*ctx.trigger_mode}
                                          : nullValue();
    case ContextField::TargetPath:
      return ctx.target_path.has_value() ? Value{*ctx.target_path} : nullValue();
    case ContextField::SessionStartedAt:
      return ctx.session_started_at.has_value() ? Value{*ctx.session_started_at}
                                                : nullValue();
  }

  return nullValue();
}

inline Value resolveExportSelectorValue(const ExportSelector& selector,
                                        const EvalContext& ctx) {
  switch (selector.field) {
    case ExportField::CapturedAt:
      return ctx.export_captured_at.has_value() ? Value{*ctx.export_captured_at}
                                                : nullValue();
  }

  return nullValue();
}

inline Value resolveSystemSelectorValue(const SystemSelector& selector,
                                        const EvalContext& ctx) {
  switch (selector.field) {
    case SystemField::Now:
      return ctx.system_now.has_value() ? Value{*ctx.system_now}
                                        : Value{std::chrono::system_clock::now()};
  }

  return nullValue();
}

inline Value resolveSelectorValue(const CompiledSelector& selector,
                                  const EvalContext& ctx) {
  if (const auto* hub_selector = std::get_if<HubSelector>(&selector.value)) {
    return resolveHubSelectorValue(*hub_selector, ctx);
  }
  if (const auto* context_selector =
          std::get_if<ContextSelector>(&selector.value)) {
    return resolveContextSelectorValue(*context_selector, ctx);
  }
  if (const auto* export_selector =
          std::get_if<ExportSelector>(&selector.value)) {
    return resolveExportSelectorValue(*export_selector, ctx);
  }
  if (const auto* system_selector =
          std::get_if<SystemSelector>(&selector.value)) {
    return resolveSystemSelectorValue(*system_selector, ctx);
  }

  return nullValue();
}

inline bool valuesEqual(const Value& lhs, const Value& rhs) {
  if (isNull(lhs) || isNull(rhs)) {
    return isNull(lhs) && isNull(rhs);
  }

  const auto lhs_numeric = asNumeric(lhs);
  const auto rhs_numeric = asNumeric(rhs);
  if (lhs_numeric.has_value() && rhs_numeric.has_value()) {
    return compareNumeric(*lhs_numeric, *rhs_numeric) == 0;
  }

  if (const auto* lhs_bool = std::get_if<bool>(&lhs)) {
    const auto* rhs_bool = std::get_if<bool>(&rhs);
    return rhs_bool != nullptr && *lhs_bool == *rhs_bool;
  }
  if (const auto* lhs_string = std::get_if<std::string>(&lhs)) {
    const auto* rhs_string = std::get_if<std::string>(&rhs);
    return rhs_string != nullptr && *lhs_string == *rhs_string;
  }
  if (const auto* lhs_timestamp = std::get_if<Timestamp>(&lhs)) {
    const auto* rhs_timestamp = std::get_if<Timestamp>(&rhs);
    return rhs_timestamp != nullptr && *lhs_timestamp == *rhs_timestamp;
  }

  return false;
}

inline std::optional<int> compareValues(const Value& lhs, const Value& rhs) {
  if (isNull(lhs) || isNull(rhs)) {
    return std::nullopt;
  }

  const auto lhs_numeric = asNumeric(lhs);
  const auto rhs_numeric = asNumeric(rhs);
  if (lhs_numeric.has_value() && rhs_numeric.has_value()) {
    return compareNumeric(*lhs_numeric, *rhs_numeric);
  }

  if (const auto* lhs_bool = std::get_if<bool>(&lhs)) {
    const auto* rhs_bool = std::get_if<bool>(&rhs);
    if (rhs_bool == nullptr) {
      return std::nullopt;
    }
    if (*lhs_bool == *rhs_bool) {
      return 0;
    }
    return *lhs_bool ? 1 : -1;
  }

  if (const auto* lhs_string = std::get_if<std::string>(&lhs)) {
    const auto* rhs_string = std::get_if<std::string>(&rhs);
    if (rhs_string == nullptr) {
      return std::nullopt;
    }
    if (*lhs_string < *rhs_string) {
      return -1;
    }
    if (*lhs_string > *rhs_string) {
      return 1;
    }
    return 0;
  }

  if (const auto* lhs_timestamp = std::get_if<Timestamp>(&lhs)) {
    const auto* rhs_timestamp = std::get_if<Timestamp>(&rhs);
    if (rhs_timestamp == nullptr) {
      return std::nullopt;
    }
    if (*lhs_timestamp < *rhs_timestamp) {
      return -1;
    }
    if (*lhs_timestamp > *rhs_timestamp) {
      return 1;
    }
    return 0;
  }

  return std::nullopt;
}

}  // namespace predicate_detail

inline bool ConditionLeaf::evaluate(const EvalContext& ctx) const {
  const Value source_value = predicate_detail::resolveSelectorValue(source, ctx);

  switch (op) {
    case PredicateOperator::Eq: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      return expected != nullptr &&
             predicate_detail::valuesEqual(source_value, *expected);
    }
    case PredicateOperator::Ne: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      return expected != nullptr &&
             !predicate_detail::valuesEqual(source_value, *expected);
    }
    case PredicateOperator::Gt: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      const auto comparison =
          expected == nullptr
              ? std::optional<int>{}
              : predicate_detail::compareValues(source_value, *expected);
      return comparison.has_value() && *comparison > 0;
    }
    case PredicateOperator::Ge: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      const auto comparison =
          expected == nullptr
              ? std::optional<int>{}
              : predicate_detail::compareValues(source_value, *expected);
      return comparison.has_value() && *comparison >= 0;
    }
    case PredicateOperator::Lt: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      const auto comparison =
          expected == nullptr
              ? std::optional<int>{}
              : predicate_detail::compareValues(source_value, *expected);
      return comparison.has_value() && *comparison < 0;
    }
    case PredicateOperator::Le: {
      const auto* expected =
          value.has_value() ? std::get_if<Value>(&*value) : nullptr;
      const auto comparison =
          expected == nullptr
              ? std::optional<int>{}
              : predicate_detail::compareValues(source_value, *expected);
      return comparison.has_value() && *comparison <= 0;
    }
    case PredicateOperator::In: {
      const auto* candidates =
          value.has_value() ? std::get_if<std::vector<Value>>(&*value) : nullptr;
      return candidates != nullptr &&
             std::any_of(candidates->begin(), candidates->end(),
                         [&](const Value& candidate) {
                           return predicate_detail::valuesEqual(source_value,
                                                                candidate);
                         });
    }
    case PredicateOperator::NotIn: {
      const auto* candidates =
          value.has_value() ? std::get_if<std::vector<Value>>(&*value) : nullptr;
      return candidates != nullptr &&
             std::none_of(candidates->begin(), candidates->end(),
                          [&](const Value& candidate) {
                            return predicate_detail::valuesEqual(source_value,
                                                                 candidate);
                          });
    }
    case PredicateOperator::IsTrue:
      return std::holds_alternative<bool>(source_value) &&
             std::get<bool>(source_value);
    case PredicateOperator::IsFalse:
      return std::holds_alternative<bool>(source_value) &&
             !std::get<bool>(source_value);
    case PredicateOperator::IsNull:
      return predicate_detail::isNull(source_value);
    case PredicateOperator::IsNotNull:
      return !predicate_detail::isNull(source_value);
  }

  return false;
}

inline bool ConditionAll::evaluate(const EvalContext& ctx) const {
  return std::all_of(children.begin(), children.end(),
                     [&](const std::unique_ptr<ConditionNode>& child) {
                       return child != nullptr && child->evaluate(ctx);
                     });
}

inline bool ConditionAny::evaluate(const EvalContext& ctx) const {
  return std::any_of(children.begin(), children.end(),
                     [&](const std::unique_ptr<ConditionNode>& child) {
                       return child != nullptr && child->evaluate(ctx);
                     });
}

inline bool ConditionNot::evaluate(const EvalContext& ctx) const {
  return child != nullptr && !child->evaluate(ctx);
}

inline bool ConditionNode::evaluate(const EvalContext& ctx) const {
  return std::visit(
      [&](const auto& node) {
        return node.evaluate(ctx);
      },
      value);
}

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
