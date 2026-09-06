#include <gtest/gtest.h>
#include "../src/FlightComputerContext.h"
#include <filesystem>
#include <iostream>

/**
 * @file test_flight_computer.cpp
 * @brief Unit tests for FlightComputerContext.
 */

TEST(FlightComputerTest, LoadScriptSuccessfully) {
    MoLab::FlightComputerContext context("scripts/vuelo_test.lua");
    EXPECT_TRUE(context.is_valid());
}
