#include <iostream>
#include "SimulationEngine.h"

int main() {
    // Configuración de la consola para UTF-8
    system("chcp 65001 > nul");

    std::cout << "[Main] Iniciando simulador." << std::endl;

    // Inicializar el motor de simulación
    SimulationEngine engine;

    if (!engine.initialize("data/initial_state.json")) {
        std::cerr << "[Main] Fallo al inicializar el motor desde el archivo de estado." << std::endl;
        return 1;
    }

    // Cargar el plugin de ejemplo
    engine.load_plugin("example_plugin.dll");
    // Correr un tick de simulación
    engine.run_tick();

    std::cout << "\n[Main] Simulador finalizado." << std::endl;
    return 0;
}
