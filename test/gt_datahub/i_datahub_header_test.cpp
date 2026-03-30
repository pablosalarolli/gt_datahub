#include "gt_datahub/i_datahub.hpp"

#include "gtest/gtest.h"

#include <type_traits>

TEST(IDataHubHeaderTest, HeaderIsSelfContained) {
  static_assert(std::has_virtual_destructor_v<gt::datahub::IDataHub>);
  SUCCEED();
}
