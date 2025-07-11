#include <iostream>
#include "SimulationEngine.h"

int main() {
    system("chcp 65001 > nul");
    std::cout << "[Main] Iniciando simulador." << std::endl;

    SimulationEngine engine;

    if (!engine.initialize("data/initial_state.json")) {
        std::cerr << "[Main] Fallo al inicializar el motor desde el archivo de estado." << std::endl;
        return 1;
    }

    engine.load_plugin("example_plugin.dll");
    engine.run_tick();

    std::cout << "\n[Main] Simulador finalizado." << std::endl;
    return 0;
}
