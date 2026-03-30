#include "gt_datahub/update_request.hpp"

#include "gtest/gtest.h"

TEST(UpdateRequestHeaderTest, HeaderIsSelfContained) {
  const gt::datahub::UpdateRequest request{};
  EXPECT_EQ(request.quality, gt::datahub::Quality::Good);
}
