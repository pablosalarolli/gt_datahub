#pragma once

#include "core/selector_parser.hpp"
#include "core/text_template_compiler.hpp"
#include "gt_datahub/i_datahub.hpp"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <expected>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace gt::datahub::core {

/**
 * Optional runtime context consumed by selector/text resolution.
 *
 * Public `resolveText()` uses only `system_now` and `export_captured_at`,
 * while file exports will later populate the remaining fields.
 */
struct TextResolveContext {
  std::optional<std::string> export_id;
  std::optional<std::uint64_t> export_session_id;
  std::optional<std::uint64_t> row_index;
  std::optional<std::string> trigger_mode;
  std::optional<std::string> target_path;
  std::optional<Timestamp> session_started_at;
  std::optional<Timestamp> export_captured_at;
  std::optional<Timestamp> system_now;
};

/**
 * Runtime resolver for canonical selectors and `${...}` text templates.
 *
 * Parsing remains separate from runtime evaluation: YAML fields are compiled at
 * bootstrap, while the public API can still resolve ad-hoc expressions on top
 * of the current hub snapshot and optional export context.
 */
class TextResolver {
 public:
  /**
   * Resolves one expression that may be either a canonical selector or a text
   * template containing `${selector}` interpolations.
   */
  static std::expected<std::string, ResolveError> resolveExpression(
      std::string_view expression, const IDataHub& hub,
      SelectorContext selector_context,
      TextResolveContext context = {}) {
    seedResolutionNow(context);

    if (expression.find("${") == std::string_view::npos) {
      if (looksLikeCanonicalSelector(expression)) {
        const auto selector =
            SelectorParser::parseCanonical(expression, selector_context);
        if (!selector.has_value()) {
          return std::unexpected(mapSelectorParseError(selector.error().message));
        }

        return resolveSelector(*selector, hub, context);
      }

      return std::string(expression);
    }

    const auto compiled =
        TextTemplateCompiler::compile(expression, selector_context);
    if (!compiled.has_value()) {
      return std::unexpected(mapSelectorParseError(compiled.error().message));
    }

    return resolveTemplate(*compiled, hub, context);
  }

  /**
   * Resolves one already-compiled canonical selector.
   */
  static std::expected<std::string, ResolveError> resolveSelector(
      const CompiledSelector& selector, const IDataHub& hub,
      const TextResolveContext& context) {
    if (const auto* hub_selector = std::get_if<HubSelector>(&selector.value)) {
      return resolveHubSelector(*hub_selector, hub);
    }

    if (const auto* context_selector =
            std::get_if<ContextSelector>(&selector.value)) {
      return resolveContextSelector(*context_selector, context);
    }

    if (const auto* export_selector =
            std::get_if<ExportSelector>(&selector.value)) {
      return resolveExportSelector(*export_selector, context);
    }

    if (const auto* system_selector =
            std::get_if<SystemSelector>(&selector.value)) {
      return resolveSystemSelector(*system_selector, context);
    }

    return std::unexpected(ResolveError{
        ResolveErrorCode::InvalidSyntax, "unsupported selector variant"});
  }

  /**
   * Resolves one already-compiled template.
   */
  static std::expected<std::string, ResolveError> resolveTemplate(
      const CompiledTextTemplate& template_text, const IDataHub& hub,
      const TextResolveContext& context) {
    std::string resolved;

    for (const auto& segment : template_text.segments) {
      if (const auto* text = std::get_if<TextSegment>(&segment)) {
        resolved += text->text;
        continue;
      }

      const auto* selector_segment = std::get_if<SelectorSegment>(&segment);
      if (selector_segment == nullptr) {
        return std::unexpected(ResolveError{
            ResolveErrorCode::InvalidSyntax,
            "template contains an unsupported segment type"});
      }

      auto selector_text =
          resolveSelector(selector_segment->selector, hub, context);
      if (!selector_text.has_value()) {
        return std::unexpected(selector_text.error());
      }

      resolved += *selector_text;
    }

    return resolved;
  }

 private:
  static std::expected<std::string, ResolveError> resolveHubSelector(
      const HubSelector& selector, const IDataHub& hub) {
    const auto state = hub.getState(selector.variable_name);
    if (!state.has_value()) {
      return std::unexpected(ResolveError{
          ResolveErrorCode::UnknownVariable,
          "unknown hub variable: " + selector.variable_name});
    }

    switch (selector.field) {
      case HubField::Value:
        if (!state->initialized) {
          return std::string{};
        }
        return serializeValue(state->value);
      case HubField::Quality:
        return serializeQuality(state->quality);
      case HubField::SourceTimestamp:
        if (!state->source_timestamp.has_value()) {
          return std::string{};
        }
        return serializeIso8601Utc(*state->source_timestamp);
      case HubField::HubTimestamp:
        if (!state->hub_timestamp.has_value()) {
          return std::string{};
        }
        return serializeIso8601Utc(*state->hub_timestamp);
      case HubField::Version:
        return std::to_string(state->version);
      case HubField::Initialized:
        return state->initialized ? std::string("true") : std::string("false");
    }

    return std::unexpected(ResolveError{
        ResolveErrorCode::UnknownField, "unsupported hub field"});
  }

  static std::expected<std::string, ResolveError> resolveContextSelector(
      const ContextSelector& selector, const TextResolveContext& context) {
    switch (selector.field) {
      case ContextField::ExportId:
        return context.export_id.value_or(std::string{});
      case ContextField::ExportSessionId:
        if (!context.export_session_id.has_value()) {
          return std::string{};
        }
        return std::to_string(*context.export_session_id);
      case ContextField::RowIndex:
        if (!context.row_index.has_value()) {
          return std::string{};
        }
        return std::to_string(*context.row_index);
      case ContextField::TriggerMode:
        return context.trigger_mode.value_or(std::string{});
      case ContextField::TargetPath:
        return context.target_path.value_or(std::string{});
      case ContextField::SessionStartedAt:
        if (!context.session_started_at.has_value()) {
          return std::string{};
        }
        return serializeIso8601Utc(*context.session_started_at);
    }

    return std::unexpected(ResolveError{
        ResolveErrorCode::UnknownField, "unsupported context field"});
  }

  static std::expected<std::string, ResolveError> resolveExportSelector(
      const ExportSelector& selector, const TextResolveContext& context) {
    switch (selector.field) {
      case ExportField::CapturedAt:
        if (!context.export_captured_at.has_value()) {
          return std::string{};
        }
        return serializeIso8601Utc(*context.export_captured_at);
    }

    return std::unexpected(ResolveError{
        ResolveErrorCode::UnknownField, "unsupported export field"});
  }

  static std::expected<std::string, ResolveError> resolveSystemSelector(
      const SystemSelector& selector, const TextResolveContext& context) {
    switch (selector.field) {
      case SystemField::Now:
        if (!context.system_now.has_value()) {
          return serializeSystemNowCompact(std::chrono::system_clock::now());
        }
        return serializeSystemNowCompact(*context.system_now);
    }

    return std::unexpected(ResolveError{
        ResolveErrorCode::UnknownField, "unsupported system field"});
  }

  static std::string serializeValue(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) {
      return {};
    }
    if (const auto* boolean = std::get_if<bool>(&value)) {
      return *boolean ? "true" : "false";
    }
    if (const auto* int32 = std::get_if<std::int32_t>(&value)) {
      return std::to_string(*int32);
    }
    if (const auto* uint32 = std::get_if<std::uint32_t>(&value)) {
      return std::to_string(*uint32);
    }
    if (const auto* int64 = std::get_if<std::int64_t>(&value)) {
      return std::to_string(*int64);
    }
    if (const auto* uint64 = std::get_if<std::uint64_t>(&value)) {
      return std::to_string(*uint64);
    }
    if (const auto* float_value = std::get_if<float>(&value)) {
      return serializeFloating(*float_value);
    }
    if (const auto* double_value = std::get_if<double>(&value)) {
      return serializeFloating(*double_value);
    }
    if (const auto* string_value = std::get_if<std::string>(&value)) {
      return *string_value;
    }
    if (const auto* timestamp = std::get_if<Timestamp>(&value)) {
      return serializeIso8601Utc(*timestamp);
    }

    return {};
  }

  static std::string serializeQuality(Quality quality) {
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

    return {};
  }

  template <typename T>
  static std::string serializeFloating(T value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    return stream.str();
  }

  static std::string serializeIso8601Utc(Timestamp timestamp) {
    using namespace std::chrono;

    const auto milliseconds_since_epoch =
        duration_cast<milliseconds>(timestamp.time_since_epoch());
    const auto seconds_since_epoch =
        duration_cast<seconds>(milliseconds_since_epoch);
    const auto milliseconds_part =
        milliseconds_since_epoch - seconds_since_epoch;

    const auto time_t_value =
        std::chrono::system_clock::to_time_t(timestamp);
    std::tm utc_tm = toUtcTm(time_t_value);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                  utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                  utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec,
                  static_cast<long long>(milliseconds_part.count()));
    return buffer;
  }

  static std::string serializeSystemNowCompact(Timestamp timestamp) {
    using namespace std::chrono;

    const auto milliseconds_since_epoch =
        duration_cast<milliseconds>(timestamp.time_since_epoch());
    const auto seconds_since_epoch =
        duration_cast<seconds>(milliseconds_since_epoch);
    const auto milliseconds_part =
        milliseconds_since_epoch - seconds_since_epoch;

    const auto time_t_value =
        std::chrono::system_clock::to_time_t(timestamp);
    std::tm utc_tm = toUtcTm(time_t_value);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer),
                  "%04d%02d%02dT%02d%02d%02d%03lld",
                  utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                  utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec,
                  static_cast<long long>(milliseconds_part.count()));
    return buffer;
  }

  static std::tm toUtcTm(std::time_t value) {
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &value);
#else
    gmtime_r(&value, &utc_tm);
#endif
    return utc_tm;
  }

  static ResolveError mapSelectorParseError(std::string message) {
    ResolveErrorCode code = ResolveErrorCode::InvalidSyntax;

    if (message.find("namespace") != std::string::npos ||
        message.find("not allowed in this context") != std::string::npos) {
      code = ResolveErrorCode::InvalidNamespace;
    } else if (message.find("field") != std::string::npos) {
      code = ResolveErrorCode::UnknownField;
    }

    return ResolveError{code, std::move(message)};
  }

  static void seedResolutionNow(TextResolveContext& context) {
    if (context.system_now.has_value() && context.export_captured_at.has_value()) {
      return;
    }

    const auto now = std::chrono::system_clock::now();
    if (!context.system_now.has_value()) {
      context.system_now = now;
    }
    if (!context.export_captured_at.has_value()) {
      context.export_captured_at = now;
    }
  }

  static bool looksLikeCanonicalSelector(std::string_view text) noexcept {
    if (text.empty() || text.find(' ') != std::string_view::npos ||
        text.find('\t') != std::string_view::npos ||
        text.find('\n') != std::string_view::npos ||
        text.find("${") != std::string_view::npos ||
        text.find('.') == std::string_view::npos) {
      return false;
    }

    const auto first_char = static_cast<unsigned char>(text.front());
    return std::isalpha(first_char) || first_char == '_';
  }
};

}  // namespace gt::datahub::core
