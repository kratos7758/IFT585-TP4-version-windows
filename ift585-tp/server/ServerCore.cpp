// =============================================================
//  ServerCore.cpp
//  IFT585 – TP4
// =============================================================
#include "ServerCore.h"
#include <iostream>
#include <csignal>
#include <unistd.h>

static ServerCore* g_instance = nullptr;

static void signalHandler(int sig) {
    std::cout << "\n[INFO] Signal " << sig << " reçu – arrêt en cours...\n";
    if (g_instance) g_instance->stop();
}

ServerCore::ServerCore(const std::string& dataDir, int udpPort, int tcpPort)
    : dataDir_(dataDir)
    , udpPort_(udpPort)
    , tcpPort_(tcpPort)
    , pm_(dataDir)
    , udpHandler_(pm_)
    , restServer_(pm_)
{
    g_instance = this;
}

ServerCore::~ServerCore() {
    stop();
    g_instance = nullptr;
}

bool ServerCore::run() {
    // Gestion des signaux pour arrêt propre
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[INFO] PersistenceManager : données chargées depuis " << dataDir_ << "/\n";

    if (!udpHandler_.start(udpPort_)) {
        std::cerr << "[ERREUR] Impossible de démarrer le serveur UDP\n";
        return false;
    }
    if (!restServer_.start(tcpPort_)) {
        std::cerr << "[ERREUR] Impossible de démarrer le serveur REST\n";
        udpHandler_.stop();
        return false;
    }

    std::cout << "[INFO] Pool de threads initialisé (4 workers)\n";
    std::cout << "[INFO] Serveur prêt. Appuyez sur Ctrl+C pour arrêter.\n";

    running_ = true;
    while (running_) {
        sleep(1);
    }

    udpHandler_.stop();
    restServer_.stop();
    std::cout << "[INFO] Serveur arrêté proprement.\n";
    return true;
}

void ServerCore::stop() {
    running_ = false;
}
