#include <iostream>
#include "SimulationEngine.h"

int main() {

    char* EXAMPLE_PLUGIN_PATH = "example_plugin.dll";
    int EXAMPLE_PLUGIN_TYPE = 0;
    char* ENVIRONMENT_PLUGIN_PATH = "environment.dll";
    int ENVIRONMENT_PLUGIN_TYPE = 0;
    char* PROGRAMMING_PLUGIN_PATH = "programming.dll";
    int PROGRAMMING_PLUGIN_TYPE = 0;
    char* PROPULSION_PLUGIN_PATH = "propulsion.dll";
    int PROPULSION_PLUGIN_TYPE = 1;
    char* AERODYNAMIC_PLUGIN_PATH = "aerodynamic.dll";
    int AERODYNAMIC_PLUGIN_TYPE = 1;
    char* STRUCTURES_PLUGIN_PATH = "structures.dll";
    int STRUCTURES_PLUGIN_TYPE = 1;

    // Configuración de la consola para UTF-8
    system("chcp 65001 > nul");

    std::cout << "[Main] Iniciando simulador." << std::endl;

    // Inicializar el motor de simulación
    SimulationEngine engine;

    if (!engine.initialize("data/initial_state.json")) {
        std::cerr << "[Main] Fallo al inicializar el motor desde el archivo de estado." << std::endl;
        return 1;
    }

    // Cargar los plugins
    engine.load_plugin("example_plugin.dll", 0);
    // Correr un tick de simulación

    for (int i = 0; i < 10; ++i) {
        std::cout << "[Main] Ejecutando tick " << (i + 1) << " de la simulación." << std::endl;
        engine.run_tick();
    }

    engine.shutdown();

    std::cout << "\n[Main] Simulador finalizado." << std::endl;
    return 0;
}
