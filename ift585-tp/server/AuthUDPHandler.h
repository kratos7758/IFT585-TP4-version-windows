#pragma once
// =============================================================
//  AuthUDPHandler.h  –  Authentification UDP stop-and-wait
//  IFT585 – TP4
//
//  Protocole :
//    Client → Serveur : AUTH_REQ {seq, username, password_hash}
//    Serveur → Client : AUTH_ACK {seq, token}   (succès)
//    Serveur → Client : AUTH_NAK {seq, message} (échec)
//
//    Stop-and-wait : le serveur acquitte chaque message et détecte
//    les doublons via le numéro de séquence. Le client retransmet
//    jusqu'à 3 fois avec un timeout de 2 secondes.
// =============================================================
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include "PersistenceManager.h"
#include "../common/UDPProtocol.h"

// État par couple (adresse:port) client
struct ClientAuthState {
    AuthState   state     = AuthState::WAITING;
    uint32_t    last_seq  = UINT32_MAX;
    std::string last_reply;  // dernier datagramme envoyé (pour retransmission)
};

class AuthUDPHandler {
public:
    explicit AuthUDPHandler(PersistenceManager& pm);
    ~AuthUDPHandler();

    // Lance le thread UDP en écoute sur le port UDP_AUTH_PORT
    bool start(int port = UDP_AUTH_PORT);
    void stop();

    bool isRunning() const { return running_; }

private:
    PersistenceManager& pm_;
    int                 sockfd_ = -1;
    std::atomic<bool>   running_{false};
    std::thread         thread_;

    std::unordered_map<std::string, ClientAuthState> states_;
    std::mutex                                        statesMu_;

    void listenLoop();
    void handleDatagram(const char* buf, ssize_t len,
                        struct sockaddr_in& clientAddr, socklen_t addrLen);

    std::string processAuthReq(const std::string& username,
                                const std::string& passwordHash,
                                uint32_t seq);
    std::string processLogoutReq(const std::string& username,
                                  const std::string& token,
                                  uint32_t seq);

    std::string buildAuthAck(uint32_t seq, const std::string& token);
    std::string buildAuthNak(uint32_t seq, const std::string& msg);
    std::string buildLogoutAck(uint32_t seq);

    static std::string addrKey(const struct sockaddr_in& addr);
};
