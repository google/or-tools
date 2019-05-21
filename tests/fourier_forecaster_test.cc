#include "gtest/gtest.h"
#include "ortools/forecaster/fourier_forecaster.h"

namespace {

// The fixture for testing class Foo.
class FourierForecasterTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  FourierForecasterTest() {
     // You can do set-up work for each test here.
  }

  ~FourierForecasterTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  // Objects declared here can be used by all tests in the test case for Foo.
};


// Tests that the Foo::Bar() method does Abc.
TEST_F(FourierForecasterTest, SimpleDFT) {
    EXPECT_EQ(1000, cubic(10));
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
