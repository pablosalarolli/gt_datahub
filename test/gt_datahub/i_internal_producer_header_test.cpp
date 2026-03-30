#include "gt_datahub/i_internal_producer.hpp"

#include "gtest/gtest.h"

#include <type_traits>

TEST(IInternalProducerHeaderTest, HeaderIsSelfContained) {
  static_assert(std::has_virtual_destructor_v<gt::datahub::IInternalProducer>);
  SUCCEED();
}
