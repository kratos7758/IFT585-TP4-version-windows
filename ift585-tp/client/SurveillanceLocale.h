#pragma once
// =============================================================
//  SurveillanceLocale.h  –  Surveillance du système de fichiers
//  IFT585 – TP4
//
//  Utilise inotify (Linux) pour détecter les créations,
//  modifications et suppressions dans le répertoire surveillé.
//  Les événements sont mis en file thread-safe et consommés
//  par SyncEngine.
// =============================================================
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

enum class FileEventType {
    CREATED,
    MODIFIED,
    DELETED
};

struct FileEvent {
    FileEventType type;
    std::string   filename;    // Nom du fichier (sans chemin)
    std::string   dirId;       // Répertoire partagé concerné
    std::string   fullPath;    // Chemin complet local
};

class SurveillanceLocale {
public:
    SurveillanceLocale() = default;
    ~SurveillanceLocale();

    // Lance la surveillance du répertoire local pour un répertoire partagé
    bool startWatch(const std::string& localPath, const std::string& dirId);
    void stop();
    bool isRunning() const { return running_; }

    // Consommation des événements (bloquant si la file est vide)
    FileEvent waitEvent();

    // Version non-bloquante (retourne false si la file est vide)
    bool tryGetEvent(FileEvent& ev);

    // Nombre d'événements en attente
    size_t pendingCount();

    // Callback optionnel (alternatif à waitEvent)
    using EventCallback = std::function<void(const FileEvent&)>;
    void setCallback(EventCallback cb) { callback_ = cb; }

private:
    std::string watchPath_;
    std::string dirId_;
    int         inotifyFd_  = -1;
    int         watchDescriptor_ = -1;
    std::atomic<bool>          running_{false};
    std::thread                thread_;
    std::queue<FileEvent>      queue_;
    std::mutex                 queueMu_;
    std::condition_variable    queueCv_;
    EventCallback              callback_;

    void watchLoop();
    void pushEvent(const FileEvent& ev);
};
