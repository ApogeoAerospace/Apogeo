#include <gtest/gtest.h>

#include "aerodynamics_module.h"

#include <iostream>
#include <sstream>

namespace {

std::string dimensionsToString(const std::vector<unsigned long long>& dims) {
    std::ostringstream out;
    for (size_t i = 0; i < dims.size(); ++i) {
        if (i != 0) {
            out << "x";
        }
        out << dims[i];
    }
    return out.str();
}

} // namespace

TEST(AerodynamicsModuleTest, OpensConfiguredHdf5DatabaseAndListsDimensions) {
    aerodynamics::AerodynamicsModule module;

    ASSERT_TRUE(module.loadFromConfig("data/defaults/default_config.json"));
    ASSERT_TRUE(module.isLoaded());

    std::cout << "[AerodynamicsModule] database=" << module.databasePath() << std::endl;

    const auto& axes = module.axes();
    ASSERT_EQ(axes.size(), 3u);

    for (const auto& axis : axes) {
        std::cout << "[AerodynamicsModule] axis " << axis.name
                  << " size=" << axis.values.size();
        if (!axis.values.empty()) {
            std::cout << " min=" << axis.values.front()
                      << " max=" << axis.values.back();
        }
        std::cout << std::endl;
    }

    EXPECT_EQ(axes[0].name, "alpha_deg");
    EXPECT_EQ(axes[1].name, "beta_deg");
    EXPECT_EQ(axes[2].name, "mach");

    EXPECT_EQ(axes[0].values.size(), 5u);
    EXPECT_EQ(axes[1].values.size(), 3u);
    EXPECT_EQ(axes[2].values.size(), 4u);

    const auto& coefficient_dimensions = module.coefficientDimensions();
    ASSERT_FALSE(coefficient_dimensions.empty());

    for (const auto& item : coefficient_dimensions) {
        std::cout << "[AerodynamicsModule] coefficient " << item.first
                  << " dims=" << dimensionsToString(item.second)
                  << std::endl;
        EXPECT_EQ(item.second, std::vector<unsigned long long>({5u, 3u, 4u}));
    }
}

TEST(AerodynamicsModuleTest, GetCoefficientsPlaceholderIsCallable) {
    aerodynamics::AerodynamicsModule module;
    ASSERT_TRUE(module.loadFromConfig("data/defaults/default_config.json"));

    const aerodynamics::AeroCoefficients coefficients = module.getCoefficients(nullptr);

    EXPECT_DOUBLE_EQ(coefficients.cx, 0.0);
    EXPECT_DOUBLE_EQ(coefficients.cy, 0.0);
    EXPECT_DOUBLE_EQ(coefficients.cz, 0.0);
    EXPECT_DOUBLE_EQ(coefficients.cl, 0.0);
    EXPECT_DOUBLE_EQ(coefficients.cm, 0.0);
    EXPECT_DOUBLE_EQ(coefficients.cn, 0.0);
}
