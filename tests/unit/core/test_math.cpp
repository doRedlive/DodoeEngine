#include <gtest/gtest.h>

#include "runtime/core/math/math.h"

namespace {

    TEST(Math, Rad2Deg) {
        EXPECT_FLOAT_EQ(dodoe::Math::Rad2Deg(dodoe::Math::PI), 180.0f);
        EXPECT_FLOAT_EQ(dodoe::Math::Rad2Deg(0.0f), 0.0f);
    }

    TEST(Math, Clamp) {
        EXPECT_EQ(dodoe::Math::Clamp(5, 0, 10), 5);
        EXPECT_EQ(dodoe::Math::Clamp(-1, 0, 10), 0);
        EXPECT_EQ(dodoe::Math::Clamp(11, 0, 10), 10);
    }

    TEST(Math, MaxMin) {
        EXPECT_EQ(dodoe::Math::Max(1, 3, 2), 3);
        EXPECT_EQ(dodoe::Math::Min(1, 3, 2), 1);
    }

    TEST(Math, RadiansDegreesRoundTrip) {
        float deg = 45.0f;
        EXPECT_NEAR(dodoe::Math::Degrees(dodoe::Math::Radians(deg)), deg, 1e-5f);
    }

}

