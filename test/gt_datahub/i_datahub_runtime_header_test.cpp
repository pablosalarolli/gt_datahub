#include "gt_datahub/i_datahub_runtime.hpp"

#include "gtest/gtest.h"

#include <type_traits>

TEST(IDataHubRuntimeHeaderTest, HeaderIsSelfContained) {
  static_assert(std::has_virtual_destructor_v<gt::datahub::IDataHubRuntime>);
  SUCCEED();
}
