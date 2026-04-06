// =============================================================
//  client/main.cpp  –  Point d'entrée du client IFT585-TP
//  IFT585 – TP4
// =============================================================
#include "ClientApp.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "=== IFT585 TP4 – Client de partage de fichiers distribué ===\n";
    ClientApp app;
    return app.run(argc, argv);
}
