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

/**
 * @file OutputManager.h
 * @brief Gestión de persistencia y exportación de resultados de simulación.
 */

namespace MoLab {

/**
 * @struct SimulationDataPoint
 * @brief Muestra de estado serializable para salidas CSV/JSON/binarias.
 */
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

/**
 * @class OutputManager
 * @brief Gestor singleton de salida de resultados y métricas.
 */
class OutputManager {
public:
    /**
     * @brief Obtiene la instancia global del gestor de salida.
     * @return Referencia única a `OutputManager`.
     */
    static OutputManager& getInstance();

    /**
     * @brief Define el directorio de salida para archivos de resultados.
     * @param dir Ruta del directorio de salida.
     */
    void setOutputDirectory(const std::string& dir);

    /**
     * @brief Habilita o deshabilita formatos de salida.
     * @param csv Activa salida CSV.
     * @param json Activa salida JSON.
     * @param binary Activa salida binaria.
     */
    void setOutputFormats(bool csv, bool json, bool binary);

    /**
     * @brief Configura el intervalo de escritura por ticks.
     * @param interval Cantidad de ticks entre registros.
     */
    void setOutputInterval(int interval); // Guardar cada N ticks

    /**
     * @brief Registra estado usando un buffer FlatBuffers.
     * @param state_buffer Buffer serializado de estado.
     * @param simulation_time Tiempo de simulación.
     * @param utc_time Tiempo UTC actual.
     * @param tick Tick actual.
     */
    void recordState(const std::vector<uint8_t>& state_buffer, double simulation_time, double utc_time, int tick); // Compatibilidad

    /**
     * @brief Registra estado usando puntero a `GeneralState`.
     * @param state Estado parseado.
     * @param simulation_time Tiempo de simulación.
     * @param utc_time Tiempo UTC actual.
     * @param tick Tick actual.
     */
    void recordState(const state_vector::GeneralState* state, double simulation_time, double utc_time, int tick);

    /**
     * @brief Registra un valor de métrica por componente.
     * @param component Nombre del componente.
     * @param metric_name Nombre de la métrica.
     * @param value Valor medido.
     */
    void recordMetrics(const std::string& component, const std::string& metric_name, double value);

    /**
     * @brief Inicializa archivos y recursos de salida.
     * @param run_name Nombre base opcional de la ejecución.
     */
    void initializeOutput(const std::string& run_name = "");

    /**
     * @brief Finaliza archivos, vacía buffers y cierra la salida.
     */
    void finalizeOutput();

    /**
     * @brief Fuerza escritura inmediata de archivos abiertos.
     */
    void flush();

    /**
     * @brief Obtiene los puntos de datos acumulados en memoria.
     * @return Referencia constante al contenedor de puntos.
     */
    const std::vector<SimulationDataPoint>& getDataPoints() const { return data_points_; }

    /**
     * @brief Imprime un resumen de resultados en el logger.
     */
    void printSummary();

private:
    OutputManager() = default;
    ~OutputManager() noexcept;

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    /**
     * @brief Extrae una muestra de salida desde un estado FlatBuffers.
     */
    SimulationDataPoint extractDataPoint(const state_vector::GeneralState* state, double time, double utc_time, int tick);

    /**
     * @brief Escribe la cabecera de columnas CSV.
     */
    void writeCSVHeader();

    /**
     * @brief Escribe una muestra en formato CSV.
     */
    void writeDataPointCSV(const SimulationDataPoint& point, int tick);

    /**
     * @brief Escribe una muestra en formato JSON.
     */
    void writeDataPointJSON(const SimulationDataPoint& point, int tick);

    /**
     * @brief Escribe métricas agregadas en el bloque JSON final.
     */
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

    /**
     * @brief Bucle de trabajo del hilo escritor asíncrono.
     */
    void writerLoop();
};

} // namespace MoLab
