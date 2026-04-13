#pragma once
// =============================================================
//  SyncEngine.h  –  Moteur de cohérence client/serveur
//  IFT585 – TP4
//
//  Algorithme :
//    1) Reçoit les événements de SurveillanceLocale
//    2) Calcule le SHA-256 du fichier modifié
//    3) POST /sync/{dir_id} avec la liste des FileMetadata locaux
//    4) Compare les hashes :
//         - hash différent côté serveur → PUT /files/{id}/{nom}
//         - fichier absent localement   → GET /files/{id}/{nom}
//    5) Un timer de 30 s déclenche également un cycle de polling
// =============================================================
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include "NetworkProvider.h"
#include "SurveillanceLocale.h"
#include "../common/FileMetadata.h"

class SyncEngine {
public:
    SyncEngine(NetworkProvider& net,
               SurveillanceLocale& surveillance,
               const std::string& localDir,
               const std::string& dirId,
               const std::string& username);
    ~SyncEngine();

    void start();
    void stop();
    bool isRunning() const { return running_; }

    // Forcer une synchronisation immédiate
    void forceSync();

    // Statut courant
    enum class Status { IDLE, SYNCING, SYNC_ERROR, OFFLINE };
    Status getStatus() const { return status_; }

    using StatusCallback = std::function<void(Status)>;
    void setStatusCallback(StatusCallback cb) { statusCb_ = cb; }

private:
    NetworkProvider&    net_;
    SurveillanceLocale& surveillance_;
    std::string         localDir_;
    std::string         dirId_;
    std::string         username_;

    std::atomic<bool>   running_{false};
    std::atomic<Status> status_{Status::IDLE};
    std::thread         eventThread_;  // consomme les événements inotify
    std::thread         timerThread_;  // polling toutes les 30 secondes

    // Cache des métadonnées locales (nom → FileMetadata)
    std::map<std::string, FileMetadata> localIndex_;
    std::mutex                          indexMu_;

    StatusCallback statusCb_;

    void eventLoop();
    void timerLoop();
    void handleEvent(const FileEvent& ev);
    void runSyncCycle();

    // ---- Opérations de sync ----
    std::vector<FileMetadata> buildLocalIndex();
    void uploadFile(const std::string& name);
    void downloadFile(const FileMetadata& meta);
    void deleteLocalFile(const std::string& name);

    void setStatus(Status s);
};
