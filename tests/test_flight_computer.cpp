#include <gtest/gtest.h>
#include "../src/FlightComputerContext.h"
#include <filesystem>
#include <iostream>

/**
 * @file test_flight_computer.cpp
 * @brief Unit tests for FlightComputerContext.
 */

TEST(FlightComputerTest, LoadScriptSuccessfully) {
    Apogeo::FlightComputerContext context("scripts/vuelo_test.lua");
    EXPECT_TRUE(context.is_valid());
}
