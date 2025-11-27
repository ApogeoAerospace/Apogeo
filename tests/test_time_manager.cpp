#include <gtest/gtest.h>
#include "TimeManager.h"
#include <thread>
#include <chrono>

using namespace MoLab;

class TimeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // TimeManager es singleton, obtenemos la instancia y la reseteamos
        auto& tm = TimeManager::getInstance();
        tm.reset();
    }

    void TearDown() override {
        // Resetear después de cada test
        auto& tm = TimeManager::getInstance();
        tm.reset();
    }
};

TEST_F(TimeManagerTest, GetSingletonInstance) {
    auto& tm1 = TimeManager::getInstance();
    auto& tm2 = TimeManager::getInstance();

    // Ambas referencias deben apuntar al mismo objeto
    EXPECT_EQ(&tm1, &tm2);
}

TEST_F(TimeManagerTest, InitialSimulationTime) {
    auto& time_manager = TimeManager::getInstance();

    // El tiempo de simulación inicial debe ser 0 después del reset
    double sim_time = time_manager.getSimulationTime();
    EXPECT_DOUBLE_EQ(sim_time, 0.0);
}

TEST_F(TimeManagerTest, UpdateSimulationTime) {
    auto& time_manager = TimeManager::getInstance();

    // Avanzar el tiempo de simulación
    double dt = 0.01;
    time_manager.updateSimulationTime(dt);

    double sim_time = time_manager.getSimulationTime();
    EXPECT_DOUBLE_EQ(sim_time, 0.01);
}

TEST_F(TimeManagerTest, MultipleUpdates) {
    auto& time_manager = TimeManager::getInstance();

    // Múltiples avances de tiempo
    double dt = 0.01;
    for (int i = 0; i < 100; ++i) {
        time_manager.updateSimulationTime(dt);
    }

    double sim_time = time_manager.getSimulationTime();
    EXPECT_NEAR(sim_time, 1.0, 0.0001);
}

TEST_F(TimeManagerTest, ResetTime) {
    auto& time_manager = TimeManager::getInstance();

    // Avanzar y luego resetear
    time_manager.updateSimulationTime(10.0);
    time_manager.reset();

    double sim_time = time_manager.getSimulationTime();
    EXPECT_DOUBLE_EQ(sim_time, 0.0);
}

TEST_F(TimeManagerTest, SetSimulationTime) {
    auto& time_manager = TimeManager::getInstance();

    // Establecer tiempo directamente
    time_manager.setSimulationTime(5.5);

    double sim_time = time_manager.getSimulationTime();
    EXPECT_DOUBLE_EQ(sim_time, 5.5);
}

TEST_F(TimeManagerTest, TimeStepConsistency) {
    auto& time_manager = TimeManager::getInstance();

    // Verificar consistencia en múltiples pasos
    double dt = 0.02;
    double expected_time = 0.0;

    for (int i = 0; i < 50; ++i) {
        time_manager.updateSimulationTime(dt);
        expected_time += dt;
    }

    double sim_time = time_manager.getSimulationTime();
    EXPECT_NEAR(sim_time, expected_time, 0.0001);
}

TEST_F(TimeManagerTest, InitializeWithCustomTime) {
    auto& time_manager = TimeManager::getInstance();

    // Inicializar con tiempo UTC personalizado
    double custom_utc = 1000.0;
    double custom_sim = 0.0;
    time_manager.initialize(custom_utc, custom_sim);

    EXPECT_DOUBLE_EQ(time_manager.getStartUTC(), custom_utc);
    EXPECT_DOUBLE_EQ(time_manager.getSimulationTime(), custom_sim);
}

TEST_F(TimeManagerTest, GetCurrentUTC) {
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize();

    // Avanzar simulación
    time_manager.updateSimulationTime(10.0);

    // UTC actual debe ser UTC inicial + tiempo de simulación
    double expected_utc = time_manager.getStartUTC() + 10.0;
    EXPECT_NEAR(time_manager.getCurrentUTC(), expected_utc, 0.001);
}

TEST_F(TimeManagerTest, SimulationTimeToUTC) {
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize(1000.0, 0.0);

    // Convertir tiempo de simulación a UTC
    double sim_time = 50.0;
    double utc_time = time_manager.simulationTimeToUTC(sim_time);

    EXPECT_DOUBLE_EQ(utc_time, 1050.0);
}

TEST_F(TimeManagerTest, UTCToSimulationTime) {
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize(1000.0, 0.0);

    // Convertir UTC a tiempo de simulación
    double utc_time = 1050.0;
    double sim_time = time_manager.utcToSimulationTime(utc_time);

    EXPECT_DOUBLE_EQ(sim_time, 50.0);
}

TEST_F(TimeManagerTest, GetCurrentUTCString) {
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize();

    // Debe retornar un string no vacío
    std::string utc_string = time_manager.getCurrentUTCString();
    EXPECT_FALSE(utc_string.empty());
}
