#include "core/text_template_compiler.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <variant>

namespace {

using gt::datahub::core::CompiledTextTemplate;
using gt::datahub::core::SelectorContext;
using gt::datahub::core::SelectorSegment;
using gt::datahub::core::TextSegment;
using gt::datahub::core::TextTemplateCompiler;

TEST(TextTemplateCompilerTest, TemplateWithInterpolationsCompiles) {
  const auto template_result = TextTemplateCompiler::compile(
      "corridas/${hub.NUMERO_CORRIDA.value}_${system.now}.csv",
      SelectorContext::FileExport);

  ASSERT_TRUE(template_result.has_value());
  const CompiledTextTemplate& compiled = *template_result;

  EXPECT_TRUE(compiled.hasInterpolations());
  ASSERT_EQ(compiled.segments.size(), std::size_t{5});
  ASSERT_TRUE(std::holds_alternative<TextSegment>(compiled.segments[0]));
  EXPECT_EQ(std::get<TextSegment>(compiled.segments[0]).text, "corridas/");
  ASSERT_TRUE(std::holds_alternative<SelectorSegment>(compiled.segments[1]));
  ASSERT_TRUE(std::holds_alternative<TextSegment>(compiled.segments[2]));
  EXPECT_EQ(std::get<TextSegment>(compiled.segments[2]).text, "_");
  ASSERT_TRUE(std::holds_alternative<SelectorSegment>(compiled.segments[3]));
  ASSERT_TRUE(std::holds_alternative<TextSegment>(compiled.segments[4]));
  EXPECT_EQ(std::get<TextSegment>(compiled.segments[4]).text, ".csv");
}

}  // namespace
