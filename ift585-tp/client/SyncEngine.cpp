// =============================================================
//  SyncEngine.cpp
//  IFT585 – TP4
// =============================================================
#include "SyncEngine.h"
#include "../common/json.h"
#include "../common/sha256.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// Intervalle de polling périodique (secondes)
static const int POLL_INTERVAL_SEC = 30;

SyncEngine::SyncEngine(NetworkProvider& net,
                        SurveillanceLocale& surveillance,
                        const std::string& localDir,
                        const std::string& dirId,
                        const std::string& username)
    : net_(net)
    , surveillance_(surveillance)
    , localDir_(localDir)
    , dirId_(dirId)
    , username_(username)
{}

SyncEngine::~SyncEngine() { stop(); }

void SyncEngine::start() {
    running_     = true;
    eventThread_ = std::thread(&SyncEngine::eventLoop, this);
    timerThread_ = std::thread(&SyncEngine::timerLoop, this);
    std::cout << "[SyncEngine] Démarré pour répertoire : " << dirId_ << "\n";
}

void SyncEngine::stop() {
    running_ = false;
    surveillance_.stop(); // débloque waitEvent()
    if (eventThread_.joinable()) eventThread_.join();
    if (timerThread_.joinable()) timerThread_.join();
}

void SyncEngine::forceSync() {
    runSyncCycle();
}

// ================================================================
//  Boucle d'événements inotify
// ================================================================
void SyncEngine::eventLoop() {
    while (running_) {
        FileEvent ev = surveillance_.waitEvent();
        if (!running_) break;
        handleEvent(ev);
    }
}

// ================================================================
//  Timer de polling (toutes les 30 s)
// ================================================================
void SyncEngine::timerLoop() {
    using namespace std::chrono;
    auto lastSync = steady_clock::now();
    while (running_) {
        std::this_thread::sleep_for(seconds(1));
        if (!running_) break;
        auto now = steady_clock::now();
        if (duration_cast<seconds>(now - lastSync).count() >= POLL_INTERVAL_SEC) {
            lastSync = now;
            runSyncCycle();
        }
    }
}

// ================================================================
//  Traitement d'un événement local
// ================================================================
void SyncEngine::handleEvent(const FileEvent& ev) {
    setStatus(Status::SYNCING);
    std::cout << "[SyncEngine] Événement : "
              << (ev.type == FileEventType::CREATED  ? "CRÉÉ"     :
                  ev.type == FileEventType::MODIFIED ? "MODIFIÉ"  : "SUPPRIMÉ")
              << " → " << ev.filename << "\n";

    if (ev.type == FileEventType::DELETED) {
        // Supprimer sur le serveur
        HttpResult res = net_.del("/files/" + dirId_ + "/" + ev.filename);
        if (res.ok())
            std::cout << "[SyncEngine] Suppression serveur OK : " << ev.filename << "\n";
        else
            std::cerr << "[SyncEngine] Suppression serveur ÉCHEC : " << ev.filename
                      << " (HTTP " << res.statusCode << ")\n";

        std::lock_guard<std::mutex> lock(indexMu_);
        localIndex_.erase(ev.filename);
    } else {
        // Créé ou modifié → uploader
        uploadFile(ev.filename);
    }
    setStatus(Status::IDLE);
}

// ================================================================
//  Cycle de synchronisation complet (POST /sync/{dir_id})
// ================================================================
void SyncEngine::runSyncCycle() {
    setStatus(Status::SYNCING);
    std::cout << "[SyncEngine] Cycle de synchronisation…\n";

    std::vector<FileMetadata> local = buildLocalIndex();

    // Sérialiser la liste locale
    Json arr = Json::array();
    for (const auto& m : local) arr.push_back(m.toJson());

    HttpResult res = net_.post("/sync/" + dirId_, arr.dump());
    if (!res.ok()) {
        std::cerr << "[SyncEngine] POST /sync/ ÉCHEC (HTTP " << res.statusCode << ")\n";
        setStatus(Status::ERROR);
        return;
    }

    Json resp;
    try { resp = Json::parse(res.body); } catch (...) { setStatus(Status::ERROR); return; }

    // Fichiers à télécharger
    if (resp.contains("to_download") && resp.at("to_download").is_array()) {
        for (const auto& e : resp.at("to_download").get_array()) {
            FileMetadata meta = FileMetadata::fromJson(e);
            downloadFile(meta);
        }
    }

    // Fichiers à uploader
    if (resp.contains("to_upload") && resp.at("to_upload").is_array()) {
        for (const auto& e : resp.at("to_upload").get_array()) {
            uploadFile(e.get_string());
        }
    }

    setStatus(Status::IDLE);
    std::cout << "[SyncEngine] Synchronisation terminée.\n";
}

// ================================================================
//  Opérations de synchronisation
// ================================================================
std::vector<FileMetadata> SyncEngine::buildLocalIndex() {
    std::vector<FileMetadata> result;
    std::lock_guard<std::mutex> lock(indexMu_);
    localIndex_.clear();

    DIR* dir = opendir(localDir_.c_str());
    if (!dir) return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;

        std::string fullPath = localDir_ + "/" + name;
        struct stat st{};
        if (stat(fullPath.c_str(), &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue; // ignorer les sous-répertoires

        FileMetadata meta;
        meta.name    = name;
        meta.dir_id  = dirId_;
        meta.hash    = SHA256::hashFile(fullPath);
        meta.size    = (long long)st.st_size;
        meta.mtime   = (long long)st.st_mtime;
        meta.deleted = false;

        localIndex_[name] = meta;
        result.push_back(meta);
    }
    closedir(dir);
    return result;
}

void SyncEngine::uploadFile(const std::string& name) {
    std::string fullPath = localDir_ + "/" + name;
    std::ifstream f(fullPath, std::ios::binary);
    if (!f) {
        std::cerr << "[SyncEngine] Impossible de lire : " << fullPath << "\n";
        return;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string data = ss.str();

    HttpResult res = net_.uploadFile(dirId_, name, data);
    if (res.ok())
        std::cout << "[SyncEngine] Upload OK : " << name << " (" << data.size() << " octets)\n";
    else
        std::cerr << "[SyncEngine] Upload ÉCHEC : " << name
                  << " (HTTP " << res.statusCode << ")\n";
}

void SyncEngine::downloadFile(const FileMetadata& meta) {
    HttpResult res = net_.downloadFile(dirId_, meta.name);
    if (!res.ok()) {
        std::cerr << "[SyncEngine] Download ÉCHEC : " << meta.name
                  << " (HTTP " << res.statusCode << ")\n";
        return;
    }

    std::string fullPath = localDir_ + "/" + meta.name;
    std::ofstream f(fullPath, std::ios::binary);
    if (!f) {
        std::cerr << "[SyncEngine] Impossible d'écrire : " << fullPath << "\n";
        return;
    }
    f.write(res.body.c_str(), (std::streamsize)res.body.size());
    std::cout << "[SyncEngine] Download OK : " << meta.name
              << " (" << res.body.size() << " octets)\n";
}

void SyncEngine::deleteLocalFile(const std::string& name) {
    std::string path = localDir_ + "/" + name;
    (void)std::remove(path.c_str());
    std::lock_guard<std::mutex> lock(indexMu_);
    localIndex_.erase(name);
}

void SyncEngine::setStatus(Status s) {
    status_ = s;
    if (statusCb_) statusCb_(s);
}
