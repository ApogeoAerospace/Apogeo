#include <gtest/gtest.h>
#include "../src/FlightComputerContext.h"
#include <filesystem>
#include <iostream>

/**
 * @file test_flight_computer.cpp
 * @brief Unit tests for FlightComputerContext.
 */

TEST(FlightComputerTest, LoadScriptSuccessfully) {
    // Instantiate FlightComputerContext with the test script path.
    MoLab::FlightComputerContext context("scripts/vuelo_test.lua");

    // Verify the script loaded without errors.
    EXPECT_TRUE(context.is_valid());
}
