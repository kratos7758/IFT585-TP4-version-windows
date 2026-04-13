// =============================================================
//  client/main.cpp  –  Point d'entrée du client IFT585-TP
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "ClientApp.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (!platform_net_init()) {
        std::cerr << "[ERREUR] Impossible d'initialiser le réseau\n";
        return 1;
    }
    std::cout << "=== IFT585 TP4 – Client de partage de fichiers distribué ===\n";
    ClientApp app;
    int ret = app.run(argc, argv);
    platform_net_cleanup();
    return ret;
}
