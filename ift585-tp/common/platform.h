#pragma once
// =============================================================
//  common/platform.h  –  Couche de compatibilité Windows/Linux
//  IFT585 – TP4
//
//  Inclure ce fichier en premier dans chaque .cpp qui utilise
//  des sockets ou des appels système (avant tout autre include).
// =============================================================

#ifdef _WIN32
// ---------------------------------------------------------------
//  Windows
// ---------------------------------------------------------------
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")

// Types manquants sur Windows
#ifndef _SSIZE_T_DEFINED
#  define _SSIZE_T_DEFINED
   typedef int       ssize_t;
#endif
#ifndef _SOCKLEN_T
#  define _SOCKLEN_T
   typedef int       socklen_t;
#endif

// Fermeture d'un descripteur de socket
inline int socket_close(int fd) { return closesocket((SOCKET)fd); }

// Fermeture du socket serveur d'écoute
#  define SHUT_RDWR SD_BOTH

// Création de répertoire
#  include <direct.h>
inline int platform_mkdir(const char* path) {
    return CreateDirectoryA(path, nullptr) ? 0 : -1;
}

// Pause d'une seconde
inline void platform_sleep_sec(int sec) { Sleep((DWORD)sec * 1000); }

// Initialisation / nettoyage Winsock (appeler une fois au démarrage)
inline bool platform_net_init() {
    WSADATA w;
    return WSAStartup(MAKEWORD(2, 2), &w) == 0;
}
inline void platform_net_cleanup() { WSACleanup(); }

// Timeout sur socket : Windows utilise DWORD (millisecondes)
inline void platform_set_recv_timeout(int fd, int sec) {
    DWORD ms = (DWORD)(sec * 1000);
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&ms), sizeof(ms));
}
inline void platform_set_send_timeout(int fd, int sec) {
    DWORD ms = (DWORD)(sec * 1000);
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&ms), sizeof(ms));
}

// Ouverture de socket (retourne int pour compatibilité avec le code existant)
inline int platform_socket(int af, int type, int proto) {
    return (int)socket(af, type, proto);
}

// Cast int → SOCKET pour les appels Winsock (no-op sur Linux)
#  define TO_SOCKET(fd) ((SOCKET)(fd))

// Génération d'octets aléatoires (UUID) via std::random_device (C++11)
#  include <random>
inline bool platform_random_bytes(unsigned char* buf, int len) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (int i = 0; i < len; i++)
        buf[i] = (unsigned char)dist(gen);
    return true;
}

// Vérification existence fichier
#  include <sys/stat.h>
inline bool platform_file_exists(const char* path) {
    struct _stat st;
    return _stat(path, &st) == 0;
}

#else
// ---------------------------------------------------------------
//  Linux / macOS
// ---------------------------------------------------------------
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <sys/select.h>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <cerrno>

inline int socket_close(int fd) { return close(fd); }

inline int platform_mkdir(const char* path) { return mkdir(path, 0755); }
inline void platform_sleep_sec(int sec) { sleep(sec); }
inline bool platform_net_init()  { return true; }
inline void platform_net_cleanup() {}

inline void platform_set_recv_timeout(int fd, int sec) {
    struct timeval tv{};
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}
inline void platform_set_send_timeout(int fd, int sec) {
    struct timeval tv{};
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

inline int platform_socket(int af, int type, int proto) {
    return socket(af, type, proto);
}

// Sur Linux, TO_SOCKET est un no-op (fd est déjà int)
#  define TO_SOCKET(fd) (fd)

inline bool platform_random_bytes(unsigned char* buf, int len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    bool ok = (read(fd, buf, len) == len);
    close(fd);
    return ok;
}

inline bool platform_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

#endif // _WIN32
