#pragma once

#include "core/selector_parser.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gt::datahub::core {

/**
 * Error returned when a textual template cannot be compiled.
 */
struct TextTemplateCompileError {
  std::string message;
};

struct TextSegment {
  std::string text;
};

struct SelectorSegment {
  CompiledSelector selector;
};

/**
 * Immutable template compiled from text and `${selector}` interpolations.
 */
struct CompiledTextTemplate {
  using Segment = std::variant<TextSegment, SelectorSegment>;

  std::vector<Segment> segments;

  /**
   * Returns whether the template contains at least one selector interpolation.
   */
  bool hasInterpolations() const noexcept {
    for (const auto& segment : segments) {
      if (std::holds_alternative<SelectorSegment>(segment)) {
        return true;
      }
    }

    return false;
  }
};

/**
 * Compiler for interpolated text fields such as `expression` and templates.
 */
class TextTemplateCompiler {
 public:
  /**
   * Compiles a text template using `${...}` interpolations and validates each
   * embedded selector against the provided execution context.
   */
  static std::expected<CompiledTextTemplate, TextTemplateCompileError> compile(
      std::string_view text, SelectorContext selector_context) {
    CompiledTextTemplate compiled;

    std::size_t cursor = 0;
    while (cursor < text.size()) {
      const auto open = text.find("${", cursor);
      if (open == std::string_view::npos) {
        pushTextSegment(compiled, text.substr(cursor));
        break;
      }

      pushTextSegment(compiled, text.substr(cursor, open - cursor));

      const auto selector_begin = open + 2;
      const auto close = text.find('}', selector_begin);
      if (close == std::string_view::npos) {
        return invalid("template interpolation is missing closing `}`");
      }

      const auto selector_text = text.substr(selector_begin, close - selector_begin);
      if (selector_text.empty()) {
        return invalid("template interpolation must not be empty");
      }

      const auto selector =
          SelectorParser::parseCanonical(selector_text, selector_context);
      if (!selector.has_value()) {
        return invalid(selector.error().message);
      }

      compiled.segments.emplace_back(SelectorSegment{std::move(*selector)});
      cursor = close + 1;
    }

    if (compiled.segments.empty()) {
      compiled.segments.emplace_back(TextSegment{std::string(text)});
    }

    return compiled;
  }

 private:
  static void pushTextSegment(CompiledTextTemplate& compiled,
                              std::string_view text) {
    if (!text.empty()) {
      compiled.segments.emplace_back(TextSegment{std::string(text)});
    }
  }

  static std::unexpected<TextTemplateCompileError> invalid(std::string message) {
    return std::unexpected(TextTemplateCompileError{std::move(message)});
  }
};

}  // namespace gt::datahub::core
