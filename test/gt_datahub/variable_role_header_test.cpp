#include "gt_datahub/variable_role.hpp"

#include "gtest/gtest.h"

TEST(VariableRoleHeaderTest, HeaderIsSelfContained) {
  const auto value = gt::datahub::VariableRole::State;
  EXPECT_EQ(value, gt::datahub::VariableRole::State);
}
