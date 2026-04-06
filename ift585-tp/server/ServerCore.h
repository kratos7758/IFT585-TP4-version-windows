#pragma once
// =============================================================
//  ServerCore.h  –  Orchestrateur principal du serveur
//  IFT585 – TP4
//
//  Responsabilités :
//    - Initialiser PersistenceManager
//    - Démarrer AuthUDPHandler (port 8888)
//    - Démarrer RESTServer (port 80)
//    - Gérer un pool de 4 threads POSIX pour les connexions TCP
// =============================================================
#include <string>
#include <atomic>
#include "PersistenceManager.h"
#include "AuthUDPHandler.h"
#include "RESTServer.h"

class ServerCore {
public:
    ServerCore(const std::string& dataDir = "./data",
               int udpPort = 8888,
               int tcpPort = 80);
    ~ServerCore();

    // Lance tous les services (bloquant jusqu'à stop())
    bool run();

    // Arrêt propre
    void stop();

private:
    std::string         dataDir_;
    int                 udpPort_;
    int                 tcpPort_;
    std::atomic<bool>   running_{false};

    PersistenceManager  pm_;
    AuthUDPHandler      udpHandler_;
    RESTServer          restServer_;
};
