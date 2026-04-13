// =============================================================
//  SurveillanceLocale.cpp  –  Surveillance du système de fichiers
//  IFT585 – TP4
//
//  Windows : ReadDirectoryChangesW (asynchrone avec OVERLAPPED)
//  Linux   : inotify
// =============================================================
#include "SurveillanceLocale.h"
#include <cstring>
#include <iostream>
#include <vector>

// ================================================================
//  Destructor / stop
// ================================================================
SurveillanceLocale::~SurveillanceLocale() {
    stop();
}

// ================================================================
//  Gestion de la file d'événements (commun Windows/Linux)
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
    return {};
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

// ================================================================
//  IMPLÉMENTATION WINDOWS
// ================================================================
#ifdef _WIN32
#include <windows.h>

bool SurveillanceLocale::startWatch(const std::string& localPath,
                                     const std::string& dirId) {
    watchPath_ = localPath;
    dirId_     = dirId;

    // Convertir le chemin en wide string pour l'API Windows
    std::wstring wpath(localPath.begin(), localPath.end());

    HANDLE hDir = CreateFileW(
        wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir == INVALID_HANDLE_VALUE) {
        std::cerr << "[SurveillanceLocale] CreateFile() ERREUR (code="
                  << GetLastError() << ") : " << localPath << "\n";
        return false;
    }

    HANDLE hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent) {
        CloseHandle(hDir);
        return false;
    }

    hDir_   = (void*)hDir;
    hEvent_ = (void*)hEvent;

    running_ = true;
    thread_  = std::thread(&SurveillanceLocale::watchLoop, this);
    std::cout << "[SurveillanceLocale] Surveillance démarrée sur : " << localPath << "\n";
    return true;
}

void SurveillanceLocale::stop() {
    running_ = false;
    if (hEvent_) {
        SetEvent((HANDLE)hEvent_);   // Débloquer WaitForSingleObject
    }
    queueCv_.notify_all();
    if (thread_.joinable()) thread_.join();

    if (hDir_)   { CloseHandle((HANDLE)hDir_);   hDir_   = nullptr; }
    if (hEvent_) { CloseHandle((HANDLE)hEvent_); hEvent_ = nullptr; }
}

void SurveillanceLocale::watchLoop() {
    HANDLE hDir   = (HANDLE)hDir_;
    HANDLE hEvent = (HANDLE)hEvent_;

    // Buffer pour les notifications (alloué sur le tas pour éviter les problèmes d'alignement)
    const DWORD BUF_SIZE = 8192;
    std::vector<char> buf(BUF_SIZE);

    DWORD dwNotifyFilter =
        FILE_NOTIFY_CHANGE_FILE_NAME  |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_SIZE;

    while (running_) {
        OVERLAPPED ov = {};
        ov.hEvent = hEvent;
        ResetEvent(hEvent);

        DWORD bytesReturned = 0;
        BOOL ok = ReadDirectoryChangesW(
            hDir,
            buf.data(), BUF_SIZE,
            FALSE,            // ne pas surveiller les sous-répertoires
            dwNotifyFilter,
            &bytesReturned,
            &ov,
            nullptr);

        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            if (running_) std::cerr << "[SurveillanceLocale] ReadDirectoryChangesW() ERREUR\n";
            break;
        }

        // Attendre avec timeout de 1 s pour vérifier running_ périodiquement
        DWORD wait = WaitForSingleObject(hEvent, 1000);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) break;
        if (!running_) break;

        if (!GetOverlappedResult(hDir, &ov, &bytesReturned, TRUE)) break;
        if (bytesReturned == 0) continue;

        // Parcourir les entrées FILE_NOTIFY_INFORMATION
        const char* ptr = buf.data();
        for (;;) {
            const FILE_NOTIFY_INFORMATION* fni =
                reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(ptr);

            // Convertir le nom de fichier (wide → UTF-8)
            int wlen = fni->FileNameLength / sizeof(WCHAR);
            int nbytes = WideCharToMultiByte(
                CP_UTF8, 0,
                fni->FileName, wlen,
                nullptr, 0, nullptr, nullptr);

            std::string filename;
            if (nbytes > 0) {
                filename.resize(nbytes);
                WideCharToMultiByte(
                    CP_UTF8, 0,
                    fni->FileName, wlen,
                    &filename[0], nbytes, nullptr, nullptr);
            }

            // Ignorer les fichiers cachés et les noms vides
            if (!filename.empty() && filename[0] != '.') {
                FileEvent ev;
                ev.filename = filename;
                ev.dirId    = dirId_;
                ev.fullPath = watchPath_ + "\\" + filename;

                switch (fni->Action) {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    ev.type = FileEventType::CREATED;
                    pushEvent(ev);
                    break;
                case FILE_ACTION_MODIFIED:
                    ev.type = FileEventType::MODIFIED;
                    pushEvent(ev);
                    break;
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                    ev.type = FileEventType::DELETED;
                    pushEvent(ev);
                    break;
                default:
                    break;
                }
            }

            if (fni->NextEntryOffset == 0) break;
            ptr += fni->NextEntryOffset;
        }
    }
}

// ================================================================
//  IMPLÉMENTATION LINUX (inotify)
// ================================================================
#else
#include <sys/inotify.h>
#include <unistd.h>
#include <cerrno>

static const int INOTIFY_BUF_LEN = 4096;

bool SurveillanceLocale::startWatch(const std::string& localPath,
                                     const std::string& dirId) {
    watchPath_ = localPath;
    dirId_     = dirId;

    inotifyFd_ = inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        std::cerr << "[SurveillanceLocale] inotify_init1() : " << strerror(errno) << "\n";
        return false;
    }

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

void SurveillanceLocale::watchLoop() {
    char buf[INOTIFY_BUF_LEN] __attribute__((aligned(__alignof__(struct inotify_event))));
    fd_set readfds;
    struct timeval tv;

    while (running_) {
        FD_ZERO(&readfds);
        FD_SET(inotifyFd_, &readfds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int ret = select(inotifyFd_ + 1, &readfds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

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
            if (filename.empty() || filename[0] == '.') continue;

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

#endif // _WIN32
