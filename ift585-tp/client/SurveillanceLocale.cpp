// =============================================================
//  SurveillanceLocale.cpp  –  Surveillance inotify (Linux)
//  IFT585 – TP4
// =============================================================
#include "SurveillanceLocale.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

// Taille du buffer inotify
static const int INOTIFY_BUF_LEN = 4096;

SurveillanceLocale::~SurveillanceLocale() {
    stop();
}

bool SurveillanceLocale::startWatch(const std::string& localPath,
                                     const std::string& dirId) {
    watchPath_ = localPath;
    dirId_     = dirId;

    inotifyFd_ = inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        std::cerr << "[SurveillanceLocale] inotify_init1() : " << strerror(errno) << "\n";
        return false;
    }

    // Surveiller : création, modification, suppression, déplacement
    uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE |
                    IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE;

    watchDescriptor_ = inotify_add_watch(inotifyFd_, localPath.c_str(), mask);
    if (watchDescriptor_ < 0) {
        std::cerr << "[SurveillanceLocale] inotify_add_watch(" << localPath
                  << ") : " << strerror(errno) << "\n";
        close(inotifyFd_); inotifyFd_ = -1;
        return false;
    }

    running_ = true;
    thread_  = std::thread(&SurveillanceLocale::watchLoop, this);
    std::cout << "[SurveillanceLocale] Surveillance démarrée sur : " << localPath << "\n";
    return true;
}

void SurveillanceLocale::stop() {
    running_ = false;
    if (inotifyFd_ >= 0) {
        if (watchDescriptor_ >= 0)
            inotify_rm_watch(inotifyFd_, watchDescriptor_);
        close(inotifyFd_);
        inotifyFd_ = -1;
        watchDescriptor_ = -1;
    }
    queueCv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

// ================================================================
//  Boucle de surveillance
// ================================================================
void SurveillanceLocale::watchLoop() {
    char buf[INOTIFY_BUF_LEN] __attribute__((aligned(__alignof__(struct inotify_event))));

    // Utilise select() pour un poll non-bloquant avec timeout
    fd_set readfds;
    struct timeval tv;

    while (running_) {
        FD_ZERO(&readfds);
        FD_SET(inotifyFd_, &readfds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int ret = select(inotifyFd_ + 1, &readfds, nullptr, nullptr, &tv);
        if (ret <= 0) continue; // timeout ou erreur

        ssize_t len = read(inotifyFd_, buf, sizeof(buf));
        if (len <= 0) {
            if (running_) std::cerr << "[SurveillanceLocale] read() : " << strerror(errno) << "\n";
            break;
        }

        ssize_t i = 0;
        while (i < len) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(buf + i);
            i += (ssize_t)(sizeof(struct inotify_event) + event->len);

            if (event->len == 0) continue;
            std::string filename(event->name);
            if (filename.empty() || filename[0] == '.') continue; // ignorer fichiers cachés

            FileEvent ev;
            ev.filename = filename;
            ev.dirId    = dirId_;
            ev.fullPath = watchPath_ + "/" + filename;

            if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                ev.type = FileEventType::CREATED;
                pushEvent(ev);
            } else if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                ev.type = FileEventType::MODIFIED;
                pushEvent(ev);
            } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                ev.type = FileEventType::DELETED;
                pushEvent(ev);
            }
        }
    }
}

// ================================================================
//  Gestion de la file d'événements
// ================================================================
void SurveillanceLocale::pushEvent(const FileEvent& ev) {
    {
        std::lock_guard<std::mutex> lock(queueMu_);
        queue_.push(ev);
    }
    queueCv_.notify_one();
    if (callback_) callback_(ev);
}

FileEvent SurveillanceLocale::waitEvent() {
    std::unique_lock<std::mutex> lock(queueMu_);
    queueCv_.wait(lock, [this]{ return !queue_.empty() || !running_; });
    if (!queue_.empty()) {
        FileEvent ev = queue_.front();
        queue_.pop();
        return ev;
    }
    return {}; // arrêt
}

bool SurveillanceLocale::tryGetEvent(FileEvent& ev) {
    std::lock_guard<std::mutex> lock(queueMu_);
    if (queue_.empty()) return false;
    ev = queue_.front();
    queue_.pop();
    return true;
}

size_t SurveillanceLocale::pendingCount() {
    std::lock_guard<std::mutex> lock(queueMu_);
    return queue_.size();
}
