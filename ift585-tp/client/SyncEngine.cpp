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
#include <filesystem>

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

    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::exists(localDir_, ec)) return result;

    for (const auto& entry : fs::directory_iterator(localDir_, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;

        std::string fullPath = entry.path().string();

        FileMetadata meta;
        meta.name    = name;
        meta.dir_id  = dirId_;
        meta.hash    = SHA256::hashFile(fullPath);
        meta.size    = (long long)entry.file_size();
        // mtime : secondes depuis l'epoch Unix (compatible C++17)
        auto ftime   = entry.last_write_time();
        auto now_fs  = fs::file_time_type::clock::now();
        auto now_sys = std::chrono::system_clock::now();
        auto mtime_sys = std::chrono::time_point_cast<std::chrono::seconds>(
                             now_sys + (ftime - now_fs));
        meta.mtime   = (long long)mtime_sys.time_since_epoch().count();
        meta.deleted = false;

        localIndex_[name] = meta;
        result.push_back(meta);
    }
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
