#include "gt_datahub/update_request.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <variant>

namespace {

using gt::datahub::Quality;
using gt::datahub::Timestamp;
using gt::datahub::UpdateRequest;
using gt::datahub::Value;

TEST(UpdateRequestTest, DefaultsMatchPublicContract) {
  const UpdateRequest request{};

  EXPECT_TRUE(std::holds_alternative<std::monostate>(request.value));
  EXPECT_EQ(request.quality, Quality::Good);
  EXPECT_FALSE(request.source_timestamp.has_value());
}

TEST(UpdateRequestTest, CanCarryProducerPayload) {
  UpdateRequest request;
  request.value = Value{std::string{"coil_a"}};
  request.quality = Quality::Bad;
  request.source_timestamp = Timestamp{std::chrono::milliseconds{200}};

  ASSERT_TRUE(std::holds_alternative<std::string>(request.value));
  EXPECT_EQ(std::get<std::string>(request.value), "coil_a");
  EXPECT_EQ(request.quality, Quality::Bad);
  ASSERT_TRUE(request.source_timestamp.has_value());
  EXPECT_EQ(*request.source_timestamp, Timestamp{std::chrono::milliseconds{200}});
}

}  // namespace
