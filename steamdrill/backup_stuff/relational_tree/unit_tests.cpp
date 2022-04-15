#include <sys/user.h>
#include <sys/reg.h>
#include <iostream>
#include "nlohmann/json.hpp"
#include "gtest/gtest.h"

#include "inspector.hpp"


TEST(InspectorInputTest, serializesMem)
{
  Extraction::InspectorInput ii(Extraction::MEMORY, 0x1000);
  nlohmann::json j(ii);

  Extraction::InspectorInput i = j.get<Extraction::InspectorInput>();
  ASSERT_EQ(i,ii);
}

TEST(InspectorInputTest, serializesReg)
{
  Extraction::InspectorInput ii(Extraction::REGISTER, UESP);
  nlohmann::json j(ii);

  Extraction::InspectorInput i = j.get<Extraction::InspectorInput>();
  ASSERT_EQ(i,ii);
}


TEST(InspectorTest, simple)
{
  Extraction::Inspector i;
  nlohmann::json j(i);

  Extraction::Inspector ii = j.get<Extraction::Inspector>();
  ASSERT_EQ(i,ii);
}

TEST(InspectorTest, constString)
{
  Extraction::Inspector i;
  i.setString("helloWorld");
  nlohmann::json j(i);

  Extraction::Inspector ii = j.get<Extraction::Inspector>();
  ASSERT_EQ(i,ii);
}

TEST(InspectorTest, function)
{
  Extraction::InspectorInput ii(Extraction::REGISTER, UESP);
  Extraction::Inspector i;
  i.setFunction("main");
  i.addInput(ii);
  nlohmann::json j(i);

  Extraction::Inspector ni = j.get<Extraction::Inspector>();
  ASSERT_EQ(i,ni);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
