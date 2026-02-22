#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <deque>
#include <thread>
#include <atomic>
#include "state_vector_generated.h"
#include "nlohmann/json.hpp"

namespace MoLab {

// Punto de datos de salida con métricas principales del estado.
struct SimulationDataPoint {
    double time;           // Tiempo de simulación (relativo)
    double utc_time;       // Tiempo UTC (absoluto)
    
    // Cinemática
    double position_x, position_y, position_z;
    double velocity_x, velocity_y, velocity_z;
    double orientation_x, orientation_y, orientation_z, orientation_w;
    double angular_velocity_x, angular_velocity_y, angular_velocity_z;
    
    // Masa y propiedades
    double total_mass;
    double cg_x, cg_y, cg_z;
    
    // Datos aerodinámicos
    double mach_number;
    double dynamic_pressure;
    double angle_of_attack;
    double sideslip_angle;
    
    // Ambiente
    double atm_density, atm_pressure, atm_temperature;
    double gravity_x, gravity_y, gravity_z;
    double wind_speed_x, wind_speed_y, wind_speed_z;
};

// Gestor de salida de resultados y métricas (singleton).
// Controla formatos (CSV/JSON/binario) y persistencia periódica.
class OutputManager {
public:
    static OutputManager& getInstance();

    // Configuración
    void setOutputDirectory(const std::string& dir);
    void setOutputFormats(bool csv, bool json, bool binary);
    void setOutputInterval(int interval); // Guardar cada N ticks

    // Registro de datos
    void recordState(const std::vector<uint8_t>& state_buffer, double simulation_time, double utc_time, int tick); // Compatibilidad
    void recordState(const state_vector::GeneralState* state, double simulation_time, double utc_time, int tick);
    void recordMetrics(const std::string& component, const std::string& metric_name, double value);

    // Operaciones de archivos
    void initializeOutput(const std::string& run_name = "");
    void finalizeOutput();
    void flush();

    // Acceso a datos
    const std::vector<SimulationDataPoint>& getDataPoints() const { return data_points_; }
    void printSummary();

private:
    OutputManager() = default;
    ~OutputManager() noexcept;

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    // Métodos auxiliares
    SimulationDataPoint extractDataPoint(const state_vector::GeneralState* state, double time, double utc_time, int tick);
    void writeCSVHeader();
    void writeDataPointCSV(const SimulationDataPoint& point, int tick);
    void writeDataPointJSON(const SimulationDataPoint& point, int tick);
    void writeMetricsJSON();

    // Configuración
    std::string output_dir_ = "output";
    bool output_csv_ = true;
    bool output_json_ = true;
    bool output_binary_ = false;
    int output_interval_ = 1; // Guardar cada tick por defecto

    // Datos en memoria
    std::vector<SimulationDataPoint> data_points_;
    std::map<std::string, std::map<std::string, std::vector<double>>> metrics_;

    // Archivos de salida
    std::unique_ptr<std::ofstream> csv_file_;
    std::unique_ptr<std::ofstream> json_file_;
    std::unique_ptr<std::ofstream> binary_file_;
    std::string run_name_;

    // Estructura para cola de escritura asíncrona
    struct PendingPoint {
        SimulationDataPoint point;
        int tick;
    };

    // Sincronización y cola de trabajo
    std::mutex data_mutex_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<PendingPoint> pending_points_;
    std::thread writer_thread_;
    std::atomic<bool> writer_running_{false};

    // Estado interno
    bool initialized_ = false;
    int last_recorded_tick_ = -1;

    // Bucle de escritura en hilo dedicado
    void writerLoop();
};

} // namespace MoLab
