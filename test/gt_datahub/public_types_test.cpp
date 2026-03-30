#include "gt_datahub/data_type.hpp"
#include "gt_datahub/errors.hpp"
#include "gt_datahub/quality.hpp"
#include "gt_datahub/timestamp.hpp"
#include "gt_datahub/variable_role.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <type_traits>

namespace {

using gt::datahub::DataType;
using gt::datahub::OpenProducerError;
using gt::datahub::OpenProducerErrorCode;
using gt::datahub::Quality;
using gt::datahub::ResolveError;
using gt::datahub::ResolveErrorCode;
using gt::datahub::RuntimeError;
using gt::datahub::RuntimeErrorCode;
using gt::datahub::SubmitError;
using gt::datahub::SubmitErrorCode;
using gt::datahub::Timestamp;
using gt::datahub::TriggerError;
using gt::datahub::TriggerErrorCode;
using gt::datahub::VariableRole;

static_assert(std::is_same_v<Timestamp, std::chrono::system_clock::time_point>);

TEST(PublicTypesTest, DataTypeSupportsConstructionAndComparison) {
  const DataType value = DataType::String;

  EXPECT_EQ(value, DataType::String);
  EXPECT_NE(value, DataType::Bool);
}

TEST(PublicTypesTest, QualitySupportsConstructionAndComparison) {
  const Quality value = Quality::Good;

  EXPECT_EQ(value, Quality::Good);
  EXPECT_NE(value, Quality::Bad);
}

TEST(PublicTypesTest, VariableRoleSupportsConstructionAndComparison) {
  const VariableRole value = VariableRole::Measurement;

  EXPECT_EQ(value, VariableRole::Measurement);
  EXPECT_NE(value, VariableRole::Alarm);
}

TEST(PublicTypesTest, TimestampAliasUsesSystemClock) {
  const Timestamp timestamp = std::chrono::system_clock::now();

  EXPECT_GE(timestamp.time_since_epoch().count(), 0);
}

TEST(PublicTypesTest, ErrorPayloadsHaveCoherentDefaults) {
  const ResolveError resolve_error{};
  const SubmitError submit_error{};
  const TriggerError trigger_error{};
  const OpenProducerError open_error{};
  const RuntimeError runtime_error{};

  EXPECT_EQ(resolve_error.code, ResolveErrorCode::InvalidSyntax);
  EXPECT_TRUE(resolve_error.message.empty());
  EXPECT_EQ(submit_error.code, SubmitErrorCode::InvalidType);
  EXPECT_TRUE(submit_error.message.empty());
  EXPECT_EQ(trigger_error.code, TriggerErrorCode::UnknownExport);
  EXPECT_TRUE(trigger_error.message.empty());
  EXPECT_EQ(open_error.code, OpenProducerErrorCode::UnknownBinding);
  EXPECT_TRUE(open_error.message.empty());
  EXPECT_EQ(runtime_error.code, RuntimeErrorCode::InvalidConfiguration);
  EXPECT_TRUE(runtime_error.message.empty());
}

TEST(PublicTypesTest, CanInstantiateSprint11Types) {
  const Timestamp timestamp = std::chrono::system_clock::now();
  const ResolveError resolve_error{ResolveErrorCode::UnknownVariable,
                                   "unknown variable"};
  const SubmitError submit_error{SubmitErrorCode::OwnershipViolation,
                                 "ownership violation"};
  const TriggerError trigger_error{TriggerErrorCode::InvalidTriggerMode,
                                   "invalid trigger"};
  const OpenProducerError open_error{OpenProducerErrorCode::AlreadyOpen,
                                     "already open"};
  const RuntimeError runtime_error{RuntimeErrorCode::AlreadyStarted,
                                   "runtime already started"};

  EXPECT_EQ(DataType::DateTime, DataType::DateTime);
  EXPECT_EQ(Quality::Uncertain, Quality::Uncertain);
  EXPECT_EQ(VariableRole::Other, VariableRole::Other);
  EXPECT_NE(timestamp.time_since_epoch().count(), 0);
  EXPECT_EQ(resolve_error.code, ResolveErrorCode::UnknownVariable);
  EXPECT_EQ(submit_error.code, SubmitErrorCode::OwnershipViolation);
  EXPECT_EQ(trigger_error.code, TriggerErrorCode::InvalidTriggerMode);
  EXPECT_EQ(open_error.code, OpenProducerErrorCode::AlreadyOpen);
  EXPECT_EQ(runtime_error.code, RuntimeErrorCode::AlreadyStarted);
}

}  // namespace
