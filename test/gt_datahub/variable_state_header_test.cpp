#include "gt_datahub/variable_state.hpp"

#include "gtest/gtest.h"

TEST(VariableStateHeaderTest, HeaderIsSelfContained) {
  const gt::datahub::VariableState state{};
  EXPECT_EQ(state.quality, gt::datahub::Quality::Uncertain);
}
