#include <gtest/gtest.h>

#include "dopch.h"

#include "runtime/core/application.h"

#include <filesystem>

namespace {

    std::filesystem::path temp_config_path(const char* file_name) {
        const auto dir = std::filesystem::temp_directory_path() / "dodoe_tests";
        std::filesystem::create_directories(dir);
        return dir / file_name;
    }

    TEST(ApplicationConfig, LoadFixture) {
        dodoe::ApplicationSpecification spec;
        EXPECT_TRUE(spec.loadFromFile(DODOE_TEST_DATA_DIR "/config/app_config.json"));
        EXPECT_STREQ(spec.name.c_str(), "TestProject");
        EXPECT_EQ(spec.width, (dodoe::UInt32)800);
        EXPECT_EQ(spec.height, (dodoe::UInt32)600);
        EXPECT_FALSE(spec.window_resizeable);
    }

    TEST(ApplicationConfig, SaveLoadRoundTrip) {
        dodoe::ApplicationSpecification out;
        out.name = "RoundTripProject";
        out.width = 1024;
        out.height = 768;
        out.window_resizeable = false;

        const auto path = temp_config_path("round_trip.json");
        ASSERT_TRUE(out.saveToFile(path));

        dodoe::ApplicationSpecification in;
        EXPECT_TRUE(in.loadFromFile(path));
        EXPECT_STREQ(in.name.c_str(), "RoundTripProject");
        EXPECT_EQ(in.width, (dodoe::UInt32)1024);
        EXPECT_EQ(in.height, (dodoe::UInt32)768);
        EXPECT_FALSE(in.window_resizeable);

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    TEST(ApplicationConfig, LoadMalformedJsonFails) {
        dodoe::ApplicationSpecification spec;
        EXPECT_FALSE(spec.loadFromFile(DODOE_TEST_DATA_DIR "/config/bad_config.json"));
    }

    TEST(ApplicationConfig, LoadMissingFileFails) {
        dodoe::ApplicationSpecification spec;
        EXPECT_FALSE(spec.loadFromFile(DODOE_TEST_DATA_DIR "/config/does_not_exist.json"));
    }

}
