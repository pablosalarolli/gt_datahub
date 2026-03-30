#include "gt_datahub/data_type.hpp"

#include "gtest/gtest.h"

TEST(DataTypeHeaderTest, HeaderIsSelfContained) {
  const auto value = gt::datahub::DataType::Bool;
  EXPECT_EQ(value, gt::datahub::DataType::Bool);
}
