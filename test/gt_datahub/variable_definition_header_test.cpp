#include "gt_datahub/variable_definition.hpp"

#include "gtest/gtest.h"

TEST(VariableDefinitionHeaderTest, HeaderIsSelfContained) {
  gt::datahub::VariableDefinition definition;
  definition.data_type = gt::datahub::DataType::Bool;
  EXPECT_EQ(definition.role, gt::datahub::VariableRole::Other);
}
