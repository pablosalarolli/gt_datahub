#include "gt_datahub/value.hpp"

#include "gtest/gtest.h"

#include <variant>

TEST(ValueHeaderTest, HeaderIsSelfContained) {
  const gt::datahub::Value value{};
  EXPECT_TRUE(std::holds_alternative<std::monostate>(value));
}
