// =============================================================
//  ClientApp.cpp
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "ClientApp.h"
#include "../common/sha256.h"
#include "../common/json.h"
#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <iostream>
#include <cstdlib>
#include <filesystem>

// ================================================================
//  Constructeur / Destructeur
// ================================================================
ClientApp::ClientApp() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");   // fallback
    localBaseDir_ = home ? std::string(home) + "\\IFT585-TP" : "C:\\IFT585-TP";
    // Remplacer les antislashes par des slashes pour cohérence interne
    for (char& c : localBaseDir_) if (c == '\\') c = '/';
#else
    const char* home = getenv("HOME");
    localBaseDir_ = home ? std::string(home) + "/IFT585-TP" : "/tmp/IFT585-TP";
#endif
    std::filesystem::create_directories(localBaseDir_);
}

ClientApp::~ClientApp() {
    disconnect();
}

// ================================================================
//  Point d'entrée
// ================================================================
int ClientApp::run(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("IFT585-TP");

    // ---- Fenêtre de connexion ----
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) return 0;

    std::string ip   = login.getServerIp();
    std::string user = login.getUsername();
    std::string pass = login.getPassword();

    // Hacher le mot de passe avant envoi
    std::string passHash = SHA256::hash(pass);

    net_.setServer(ip, 8888, 8080);
    if (!connectToServer(ip, user, passHash)) {
        QMessageBox::critical(nullptr, "Erreur",
            "Connexion échouée après 3 tentatives.\nVérifiez l'IP et les identifiants.");
        return 1;
    }

    // ---- Fenêtre principale ----
    mainWin_ = new MainWindow();
    mainWin_->setUsername(username_);

    // Brancher les callbacks GUI → ClientApp
    mainWin_->setOnCreateDir([this](const std::string& n){ createDirectory(n); });
    mainWin_->setOnInviteUser([this](const std::string& d, const std::string& u){ inviteUser(d,u); });
    mainWin_->setOnRemoveMember([this](const std::string& d, const std::string& u){ removeMember(d,u); });
    mainWin_->setOnTransferAdmin([this](const std::string& d, const std::string& u){ transferAdmin(d,u); });
    mainWin_->setOnAcceptInvitation([this](const std::string& i){ acceptInvitation(i); });
    mainWin_->setOnRefreshDirs([this](){ refreshUI(); });
    mainWin_->setOnLogout([this](){
        disconnect();
        QApplication::quit();
    });

    // Valider la session REST
    Json loginBody = Json::object();
    loginBody["user"]  = Json(username_);
    loginBody["token"] = Json(net_.getToken());
    net_.post("/auth/login", loginBody.dump());

    refreshUI();
    mainWin_->show();
    mainWin_->appendTransferLog("Session ouverte – bienvenue " + username_);

    return app.exec();
}

// ================================================================
//  Connexion / Déconnexion
// ================================================================
bool ClientApp::connectToServer(const std::string& /*serverIp*/,
                                 const std::string& user,
                                 const std::string& passHash) {
    username_ = user;
    std::string token = net_.authenticate(user, passHash);
    return !token.empty();
}

void ClientApp::disconnect() {
    // Arrêter tous les moteurs de sync
    for (auto& [_id, eng] : syncEngines_) eng->stop();
    for (auto& [_id, w]   : watchers_)   w->stop();
    syncEngines_.clear();
    watchers_.clear();

    // Notifier le serveur via UDP
    net_.logout(username_);
    std::cout << "[ClientApp] Déconnecté proprement.\n";
}

// ================================================================
//  Actions utilisateur
// ================================================================
void ClientApp::createDirectory(const std::string& name) {
    Json body = Json::object();
    body["name"] = Json(name);
    HttpResult res = net_.post("/directories", body.dump());
    if (res.ok()) {
        mainWin_->appendTransferLog("Répertoire créé : " + name);
        refreshUI();
    } else {
        mainWin_->appendTransferLog("Erreur création répertoire : HTTP " + std::to_string(res.statusCode));
    }
}

void ClientApp::inviteUser(const std::string& dirId, const std::string& user) {
    Json body = Json::object();
    body["user_id"] = Json(user);
    HttpResult res = net_.post("/directories/" + dirId + "/invitations", body.dump());
    if (res.ok())
        mainWin_->appendTransferLog("Invitation envoyée à " + user);
    else
        mainWin_->appendTransferLog("Erreur invitation : HTTP " + std::to_string(res.statusCode));
}

void ClientApp::removeMember(const std::string& dirId, const std::string& user) {
    HttpResult res = net_.del("/directories/" + dirId + "/members/" + user);
    if (res.ok()) {
        mainWin_->appendTransferLog(user + " retiré du répertoire");
        refreshUI();
    } else {
        mainWin_->appendTransferLog("Erreur retrait membre : HTTP " + std::to_string(res.statusCode));
    }
}

void ClientApp::transferAdmin(const std::string& dirId, const std::string& user) {
    Json body = Json::object();
    body["user_id"] = Json(user);
    HttpResult res = net_.put("/directories/" + dirId + "/admin", body.dump());
    if (res.ok()) {
        mainWin_->appendTransferLog("Administrateur transféré à " + user);
        refreshUI();
    } else {
        mainWin_->appendTransferLog("Erreur transfert admin : HTTP " + std::to_string(res.statusCode));
    }
}

void ClientApp::acceptInvitation(const std::string& invId) {
    HttpResult res = net_.post("/invitations/" + invId + "/accept", "{}");
    if (res.ok()) {
        mainWin_->appendTransferLog("Invitation acceptée");
        refreshUI();
    } else {
        mainWin_->appendTransferLog("Erreur acceptation invitation");
    }
}

// ================================================================
//  Rafraîchissement de l'interface
// ================================================================
void ClientApp::refreshUI() {
    if (!mainWin_) return;

    // ---- Répertoires ----
    HttpResult dirRes = net_.get("/directories");
    if (dirRes.ok()) {
        Json resp;
        try { resp = Json::parse(dirRes.body); } catch (...) {}
        if (resp.contains("directories") && resp.at("directories").is_array()) {
            QStringList names, ids;
            for (const auto& d : resp.at("directories").get_array()) {
                names << QString::fromStdString(d.at("name").get_string());
                ids   << QString::fromStdString(d.at("id").get_string());

                std::string dirId = d.at("id").get_string();
                ensureLocalDir(dirId);
                if (syncEngines_.find(dirId) == syncEngines_.end())
                    startSyncForDir(dirId);

                // Membres
                if (d.contains("members")) {
                    QStringList members;
                    for (const auto& m : d.at("members").get_array())
                        members << QString::fromStdString(m.get_string());
                    mainWin_->setMembers(members);
                }
            }
            mainWin_->setDirectories(names, ids);
        }
    } else {
        mainWin_->setSyncStatus("offline");
        return;
    }

    // ---- Invitations ----
    HttpResult invRes = net_.get("/invitations");
    if (invRes.ok()) {
        Json resp;
        try { resp = Json::parse(invRes.body); } catch (...) {}
        if (resp.contains("invitations") && resp.at("invitations").is_array()) {
            QStringList labels, ids;
            for (const auto& inv : resp.at("invitations").get_array()) {
                QString label = QString("De %1 → %2")
                    .arg(QString::fromStdString(inv.at("from_user").get_string()))
                    .arg(QString::fromStdString(inv.at("directory_id").get_string()));
                labels << label;
                ids    << QString::fromStdString(inv.at("id").get_string());
            }
            mainWin_->setInvitations(labels, ids);
        }
    }

    mainWin_->setSyncStatus("idle");
}

// ================================================================
//  Helpers
// ================================================================
std::string ClientApp::localPath(const std::string& dirId) const {
    return localBaseDir_ + "/" + dirId;
}

void ClientApp::ensureLocalDir(const std::string& dirId) {
    std::filesystem::create_directories(localPath(dirId));
}

void ClientApp::startSyncForDir(const std::string& dirId) {
    std::string path = localPath(dirId);

    auto watcher = std::make_unique<SurveillanceLocale>();
    if (!watcher->startWatch(path, dirId)) {
        std::cerr << "[ClientApp] Impossible de surveiller : " << path << "\n";
        return;
    }

    auto engine = std::make_unique<SyncEngine>(net_, *watcher, path, dirId, username_);

    // Callback de statut → GUI
    engine->setStatusCallback([this](SyncEngine::Status s) {
        if (!mainWin_) return;
        switch (s) {
        case SyncEngine::Status::SYNCING: mainWin_->setSyncStatus("syncing"); break;
        case SyncEngine::Status::SYNC_ERROR:
        case SyncEngine::Status::OFFLINE: mainWin_->setSyncStatus("offline"); break;
        default: mainWin_->setSyncStatus("idle"); break;
        }
    });

    engine->start();

    watchers_[dirId]    = std::move(watcher);
    syncEngines_[dirId] = std::move(engine);

    // Sync initiale
    syncEngines_[dirId]->forceSync();
    mainWin_->appendTransferLog("Sync démarrée pour répertoire " + dirId);
}
