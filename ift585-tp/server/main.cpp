// =============================================================
//  server/main.cpp  –  Point d'entrée du serveur IFT585-TP
//  IFT585 – TP4
// =============================================================
#include "ServerCore.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string dataDir = "./data";
    int udpPort = 8888;
    int tcpPort = 80;

    // Lecture des arguments optionnels
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--udp-port" && i + 1 < argc) {
            udpPort = std::atoi(argv[++i]);
        } else if (arg == "--tcp-port" && i + 1 < argc) {
            tcpPort = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [--data DIR] [--udp-port PORT] [--tcp-port PORT]\n"
                      << "  --data      Répertoire des données (défaut: ./data)\n"
                      << "  --udp-port  Port UDP authentification (défaut: 8888)\n"
                      << "  --tcp-port  Port TCP REST (défaut: 80)\n";
            return 0;
        }
    }

    std::cout << "=== IFT585 TP4 – Serveur de partage de fichiers distribué ===\n";
    std::cout << "[CONFIG] Données : " << dataDir << "\n";
    std::cout << "[CONFIG] Port UDP : " << udpPort << "\n";
    std::cout << "[CONFIG] Port TCP : " << tcpPort << "\n\n";

    ServerCore server(dataDir, udpPort, tcpPort);
    return server.run() ? 0 : 1;
}
