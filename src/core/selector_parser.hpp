#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace gt::datahub::core {

/**
 * Contexts that constrain which selector namespaces are legal.
 */
enum class SelectorContext {
  PublicResolve,
  FileExport,
  FilePathTemplate
};

/**
 * Error returned when a selector cannot be compiled.
 */
struct SelectorParseError {
  std::string message;
};

enum class HubField {
  Value,
  Quality,
  SourceTimestamp,
  HubTimestamp,
  Version,
  Initialized
};

enum class ContextField {
  ExportId,
  ExportSessionId,
  RowIndex,
  TriggerMode,
  TargetPath,
  SessionStartedAt
};

enum class ExportField { CapturedAt };

enum class SystemField { Now };

struct HubSelector {
  std::string variable_name;
  HubField field;
};

struct ContextSelector {
  ContextField field;
};

struct ExportSelector {
  ExportField field;
};

struct SystemSelector {
  SystemField field;
};

/**
 * Immutable selector compiled from canonical YAML syntax.
 */
struct CompiledSelector {
  using Variant =
      std::variant<HubSelector, ContextSelector, ExportSelector, SystemSelector>;

  Variant value;

  /**
   * Returns the referenced hub variable name when this is a `hub.*` selector.
   */
  std::optional<std::string_view> hubVariableName() const noexcept {
    if (const auto* hub = std::get_if<HubSelector>(&value)) {
      return hub->variable_name;
    }

    return std::nullopt;
  }
};

/**
 * Parser for canonical selectors like `hub.VAR.value`.
 */
class SelectorParser {
 public:
  /**
   * Compiles one canonical selector and validates namespace and field names
   * according to the provided execution context.
   */
  static std::expected<CompiledSelector, SelectorParseError> parseCanonical(
      std::string_view text, SelectorContext context) {
    if (text.empty()) {
      return invalid("selector must not be empty");
    }

    const auto first_dot = text.find('.');
    if (first_dot == std::string_view::npos) {
      return invalid("selector must use canonical dot syntax");
    }

    const auto namespace_part = text.substr(0, first_dot);
    const auto remainder = text.substr(first_dot + 1);
    if (remainder.empty()) {
      return invalid("selector is incomplete after namespace");
    }

    if (namespace_part == "hub") {
      if (!isNamespaceAllowed(namespace_part, context)) {
        return invalid("namespace `hub` is not allowed in this context");
      }
      return parseHubSelector(remainder);
    }

    if (namespace_part == "context") {
      if (!isNamespaceAllowed(namespace_part, context)) {
        return invalid("namespace `context` is not allowed in this context");
      }
      return parseContextSelector(remainder);
    }

    if (namespace_part == "export") {
      if (!isNamespaceAllowed(namespace_part, context)) {
        return invalid("namespace `export` is not allowed in this context");
      }
      return parseExportSelector(remainder);
    }

    if (namespace_part == "system") {
      if (!isNamespaceAllowed(namespace_part, context)) {
        return invalid("namespace `system` is not allowed in this context");
      }
      return parseSystemSelector(remainder);
    }

    return invalid("unsupported selector namespace: " + std::string(namespace_part));
  }

 private:
  static std::expected<CompiledSelector, SelectorParseError> parseHubSelector(
      std::string_view remainder) {
    const auto last_dot = remainder.rfind('.');
    if (last_dot == std::string_view::npos) {
      return invalid("hub selector must use `hub.<variable>.<field>`");
    }

    const auto variable_name = remainder.substr(0, last_dot);
    const auto field_part = remainder.substr(last_dot + 1);
    if (variable_name.empty() || field_part.empty()) {
      return invalid("hub selector must use `hub.<variable>.<field>`");
    }

    if (field_part == "value") {
      return CompiledSelector{HubSelector{std::string(variable_name), HubField::Value}};
    }
    if (field_part == "quality") {
      return CompiledSelector{
          HubSelector{std::string(variable_name), HubField::Quality}};
    }
    if (field_part == "source_timestamp") {
      return CompiledSelector{HubSelector{std::string(variable_name),
                                          HubField::SourceTimestamp}};
    }
    if (field_part == "hub_timestamp") {
      return CompiledSelector{
          HubSelector{std::string(variable_name), HubField::HubTimestamp}};
    }
    if (field_part == "version") {
      return CompiledSelector{
          HubSelector{std::string(variable_name), HubField::Version}};
    }
    if (field_part == "initialized") {
      return CompiledSelector{
          HubSelector{std::string(variable_name), HubField::Initialized}};
    }

    return invalid("unsupported hub field: " + std::string(field_part));
  }

  static std::expected<CompiledSelector, SelectorParseError> parseContextSelector(
      std::string_view remainder) {
    if (remainder.find('.') != std::string_view::npos) {
      return invalid("context selector must use `context.<field>`");
    }

    if (remainder == "export_id") {
      return CompiledSelector{ContextSelector{ContextField::ExportId}};
    }
    if (remainder == "export_session_id") {
      return CompiledSelector{
          ContextSelector{ContextField::ExportSessionId}};
    }
    if (remainder == "row_index") {
      return CompiledSelector{ContextSelector{ContextField::RowIndex}};
    }
    if (remainder == "trigger_mode") {
      return CompiledSelector{ContextSelector{ContextField::TriggerMode}};
    }
    if (remainder == "target_path") {
      return CompiledSelector{ContextSelector{ContextField::TargetPath}};
    }
    if (remainder == "session_started_at") {
      return CompiledSelector{
          ContextSelector{ContextField::SessionStartedAt}};
    }

    return invalid("unsupported context field: " + std::string(remainder));
  }

  static std::expected<CompiledSelector, SelectorParseError> parseExportSelector(
      std::string_view remainder) {
    if (remainder.find('.') != std::string_view::npos) {
      return invalid("export selector must use `export.<field>`");
    }

    if (remainder == "captured_at") {
      return CompiledSelector{ExportSelector{ExportField::CapturedAt}};
    }

    return invalid("unsupported export field: " + std::string(remainder));
  }

  static std::expected<CompiledSelector, SelectorParseError> parseSystemSelector(
      std::string_view remainder) {
    if (remainder.find('.') != std::string_view::npos) {
      return invalid("system selector must use `system.<field>`");
    }

    if (remainder == "now") {
      return CompiledSelector{SystemSelector{SystemField::Now}};
    }

    return invalid("unsupported system field: " + std::string(remainder));
  }

  static bool isNamespaceAllowed(std::string_view namespace_part,
                                 SelectorContext context) noexcept {
    switch (context) {
      case SelectorContext::PublicResolve:
        return namespace_part == "hub" || namespace_part == "export" ||
               namespace_part == "system";
      case SelectorContext::FileExport:
        return namespace_part == "hub" || namespace_part == "export" ||
               namespace_part == "system" || namespace_part == "context";
      case SelectorContext::FilePathTemplate:
        return namespace_part == "hub" || namespace_part == "system";
    }

    return false;
  }

  static std::unexpected<SelectorParseError> invalid(std::string message) {
    return std::unexpected(SelectorParseError{std::move(message)});
  }
};

}  // namespace gt::datahub::core
