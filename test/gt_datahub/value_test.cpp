#include "gt_datahub/value.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

using gt::datahub::Timestamp;
using gt::datahub::Value;

template <typename T>
void expectRoundTrip(const T& input) {
  const Value value{input};
  const T* extracted = std::get_if<T>(&value);

  ASSERT_NE(extracted, nullptr);
  EXPECT_EQ(*extracted, input);
}

TEST(ValueTest, SupportsRoundTripForCanonicalAlternatives) {
  expectRoundTrip<bool>(true);
  expectRoundTrip<std::int32_t>(-42);
  expectRoundTrip<std::uint32_t>(42u);
  expectRoundTrip<std::int64_t>(-4200);
  expectRoundTrip<std::uint64_t>(4200u);
  expectRoundTrip<float>(1.5f);
  expectRoundTrip<double>(2.5);
  expectRoundTrip<std::string>("heat_01");
  expectRoundTrip<Timestamp>(Timestamp{std::chrono::milliseconds{1234}});
}

TEST(ValueTest, WrongAccessFailsCleanly) {
  const Value value{std::int32_t{10}};

  EXPECT_EQ(std::get_if<std::string>(&value), nullptr);
  EXPECT_THROW((void)std::get<std::string>(value), std::bad_variant_access);
}

TEST(ValueTest, CopyAndMovePreserveSelectedAlternative) {
  Value original{std::string{"payload"}};
  const Value copied{original};
  Value moved_target{std::move(original)};

  ASSERT_TRUE(std::holds_alternative<std::string>(copied));
  EXPECT_EQ(std::get<std::string>(copied), "payload");
  ASSERT_TRUE(std::holds_alternative<std::string>(moved_target));
  EXPECT_EQ(std::get<std::string>(moved_target), "payload");
  EXPECT_EQ(original.index(), moved_target.index());
}

TEST(ValueTest, DefaultConstructedValueStartsAsMonostate) {
  const Value value{};

  EXPECT_TRUE(std::holds_alternative<std::monostate>(value));
}

}  // namespace
