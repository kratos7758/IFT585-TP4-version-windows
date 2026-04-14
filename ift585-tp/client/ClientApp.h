#pragma once
// =============================================================
//  ClientApp.h  –  Coordinateur principal du client
//  IFT585 – TP4
//
//  Orchestre :
//    - NetworkProvider  (UDP auth + REST)
//    - SurveillanceLocale (inotify)
//    - SyncEngine       (cohérence)
//    - InterfaceUtilisateur (Qt5 GUI)
// =============================================================
#include <string>
#include <map>
#include <memory>
#include "NetworkProvider.h"
#include "SurveillanceLocale.h"
#include "SyncEngine.h"
#include "InterfaceUtilisateur.h"

class ClientApp {
public:
    ClientApp();
    ~ClientApp();

    // Point d'entrée principal (appelé depuis main.cpp après QApplication)
    int run(int argc, char* argv[]);

private:
    // ---- État session ----
    std::string username_;
    std::string serverIp_;
    std::string localBaseDir_; // ~/IFT585-TP/

    // ---- Modules ----
    NetworkProvider net_;
    std::map<std::string, std::unique_ptr<SurveillanceLocale>> watchers_;
    std::map<std::string, std::unique_ptr<SyncEngine>>         syncEngines_;

    // ---- GUI ----
    MainWindow* mainWin_ = nullptr;

    // ---- Connexion ----
    bool connectToServer(const std::string& ip,
                          const std::string& user,
                          const std::string& password);
    void disconnect();

    // ---- Actions utilisateur (callbacks GUI) ----
    void createDirectory(const std::string& name);
    void inviteUser(const std::string& dirId, const std::string& user);
    void removeMember(const std::string& dirId, const std::string& user);
    void transferAdmin(const std::string& dirId, const std::string& user);
    void acceptInvitation(const std::string& invId);
    void declineInvitation(const std::string& invId);
    void refreshUI();

    // ---- Helpers ----
    void ensureLocalDir(const std::string& dirId);
    void startSyncForDir(const std::string& dirId);
    std::string localPath(const std::string& dirId) const;
};
