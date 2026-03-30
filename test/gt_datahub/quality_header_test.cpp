#include "gt_datahub/quality.hpp"

#include "gtest/gtest.h"

TEST(QualityHeaderTest, HeaderIsSelfContained) {
  const auto value = gt::datahub::Quality::Good;
  EXPECT_EQ(value, gt::datahub::Quality::Good);
}
