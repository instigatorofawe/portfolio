#include <gtest/gtest.h>

#include <dsplib/dsplib.hpp>

namespace {

TEST(DbToLinear, UnityGainIsZeroDb) { EXPECT_DOUBLE_EQ(dsplib::db_to_linear(0.0), 1.0); }

TEST(DbToLinear, TwentyDbIsTenfold) { EXPECT_DOUBLE_EQ(dsplib::db_to_linear(20.0), 10.0); }

TEST(LinearToDb, RoundTrips) { EXPECT_NEAR(dsplib::linear_to_db(dsplib::db_to_linear(-6.0)), -6.0, 1e-9); }

}  // namespace
