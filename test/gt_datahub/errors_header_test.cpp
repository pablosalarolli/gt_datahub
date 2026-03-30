#include "gt_datahub/errors.hpp"

#include "gtest/gtest.h"

TEST(ErrorsHeaderTest, HeaderIsSelfContained) {
  const gt::datahub::ResolveError error{};
  EXPECT_EQ(error.code, gt::datahub::ResolveErrorCode::InvalidSyntax);
  EXPECT_TRUE(error.message.empty());
}
