#include <iostream>
#include "../core/SimulationEngine.h"

int main() {

    std::string EXAMPLE_PLUGIN_NAME = "example_plugin";
    int EXAMPLE_PLUGIN_TYPE = 0;

    std::string ENVIRONMENT_PLUGIN_NAME = "environment";
    int ENVIRONMENT_PLUGIN_TYPE = 0;

    std::string PROGRAMMING_PLUGIN_NAME = "programming";
    int PROGRAMMING_PLUGIN_TYPE = 0;

    std::string PROPULSION_PLUGIN_NAME = "propulsion";
    int PROPULSION_PLUGIN_TYPE = 1;

    std::string AERODYNAMIC_PLUGIN_NAME = "aerodynamic";
    int AERODYNAMIC_PLUGIN_TYPE = 1;

    std::string STRUCTURES_PLUGIN_NAME = "structures";
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
    engine.load_plugin(EXAMPLE_PLUGIN_NAME, EXAMPLE_PLUGIN_TYPE); //TODO: Soporte para MacOS

    // Correr un tick de simulación

    for (int i = 0; i < 10; ++i) {
        std::cout << "[Main] Ejecutando tick " << (i + 1) << " de la simulación." << std::endl;
        engine.run_tick();
    }

    engine.shutdown();

    std::cout << "\n[Main] Simulador finalizado." << std::endl;
    return 0;
}
