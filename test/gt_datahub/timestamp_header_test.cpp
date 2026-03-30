#include "gt_datahub/timestamp.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <type_traits>

TEST(TimestampHeaderTest, HeaderIsSelfContained) {
  static_assert(
      std::is_same_v<gt::datahub::Timestamp,
                     std::chrono::system_clock::time_point>);
  SUCCEED();
}
