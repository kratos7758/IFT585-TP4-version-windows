// =============================================================
//  AuthUDPHandler.cpp
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "AuthUDPHandler.h"
#include "../common/json.h"
#include "../common/sha256.h"
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>

AuthUDPHandler::AuthUDPHandler(PersistenceManager& pm) : pm_(pm) {}

AuthUDPHandler::~AuthUDPHandler() { stop(); }

// ================================================================
//  Démarrage
// ================================================================
bool AuthUDPHandler::start(int port) {
    sockfd_ = platform_socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "[AuthUDP] socket() erreur\n";
        return false;
    }

    int opt = 1;
    setsockopt(TO_SOCKET(sockfd_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(TO_SOCKET(sockfd_), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[AuthUDP] bind() erreur\n";
        socket_close(sockfd_); sockfd_ = -1;
        return false;
    }

    running_ = true;
    thread_  = std::thread(&AuthUDPHandler::listenLoop, this);
    std::cout << "[INFO] Serveur UDP en écoute sur le port " << port << "\n";
    return true;
}

void AuthUDPHandler::stop() {
    running_ = false;
    if (sockfd_ >= 0) {
        shutdown(TO_SOCKET(sockfd_), SHUT_RDWR);
        socket_close(sockfd_);
        sockfd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

// ================================================================
//  Boucle de réception
// ================================================================
void AuthUDPHandler::listenLoop() {
    char buf[UDP_MAX_PKT_SIZE];
    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        int n = (int)recvfrom(TO_SOCKET(sockfd_), buf, (int)(sizeof(buf) - 1), 0,
                              (struct sockaddr*)&clientAddr, &addrLen);
        if (n <= 0) {
            if (running_) std::cerr << "[AuthUDP] recvfrom() erreur\n";
            break;
        }
        buf[n] = '\0';
        handleDatagram(buf, n, clientAddr, addrLen);
    }
}

// ================================================================
//  Traitement d'un datagramme
// ================================================================
void AuthUDPHandler::handleDatagram(const char* buf, ssize_t /*len*/,
                                     struct sockaddr_in& clientAddr,
                                     socklen_t addrLen) {
    Json msg;
    try { msg = Json::parse(std::string(buf)); }
    catch (...) {
        std::cerr << "[AuthUDP] JSON invalide\n"; return;
    }

    if (!msg.contains("type") || !msg.contains("seq")) return;

    std::string type = msg.at("type").get_string();
    uint32_t    seq  = (uint32_t)msg.at("seq").get_int();
    std::string key  = addrKey(clientAddr);

    std::lock_guard<std::mutex> lock(statesMu_);
    ClientAuthState& st = states_[key];

    // Détection de doublon (stop-and-wait) : renvoyer le dernier ACK
    if (seq == st.last_seq && !st.last_reply.empty()) {
        sendto(TO_SOCKET(sockfd_), st.last_reply.c_str(), (int)st.last_reply.size(), 0,
               (struct sockaddr*)&clientAddr, addrLen);
        return;
    }

    std::string reply;

    if (type == MSG_AUTH_REQ) {
        if (!msg.contains("username") || !msg.contains("password_hash")) return;
        std::string username = msg.at("username").get_string();
        std::string pwHash   = msg.at("password_hash").get_string();
        // Nettoyer les caractères de contrôle éventuels (\r, \n, espaces)
        while (!username.empty() && (username.back() == '\r' || username.back() == '\n'
                                     || username.back() == ' '))
            username.pop_back();
        reply = processAuthReq(username, pwHash, seq);
    }
    else if (type == MSG_LOGOUT_REQ) {
        if (!msg.contains("username") || !msg.contains("token")) return;
        std::string username = msg.at("username").get_string();
        std::string token    = msg.at("token").get_string();
        while (!username.empty() && (username.back() == '\r' || username.back() == '\n'
                                     || username.back() == ' '))
            username.pop_back();
        reply = processLogoutReq(username, token, seq);
    }
    else {
        return; // type inconnu
    }

    st.last_seq   = seq;
    st.last_reply = reply;

    sendto(TO_SOCKET(sockfd_), reply.c_str(), (int)reply.size(), 0,
           (struct sockaddr*)&clientAddr, addrLen);
}

// ================================================================
//  Logique métier
// ================================================================
std::string AuthUDPHandler::processAuthReq(const std::string& username,
                                             const std::string& passwordHash,
                                             uint32_t seq) {
    if (!pm_.clientExists(username)) {
        // Auto-enregistrement si le client est inconnu (première connexion)
        // En production on désactiverait cela.
        ClientProfile c;
        c.username      = username;
        c.password_hash = passwordHash;
        c.status        = "offline";
        c.session_token = "";
        c.token_expiry  = 0;
        pm_.saveClient(c);
        std::cout << "[AuthUDP] Nouvel utilisateur enregistré : " << username << "\n";
    }

    ClientProfile c = pm_.getClient(username);
    if (c.password_hash != passwordHash) {
        std::cout << "[AuthUDP] Échec authentification pour : " << username << "\n";
        return buildAuthNak(seq, "Identifiants invalides");
    }

    // Générer un nouveau SessionToken (UUID v4)
    unsigned char ubuf[16];
    if (!platform_random_bytes(ubuf, 16)) {
        srand((unsigned)time(nullptr));
        for (auto& b : ubuf) b = (unsigned char)(rand() & 0xff);
    }
    ubuf[6] = (ubuf[6] & 0x0f) | 0x40;
    ubuf[8] = (ubuf[8] & 0x3f) | 0x80;
    char uuidBuf[37];
    snprintf(uuidBuf, sizeof(uuidBuf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        ubuf[0],ubuf[1],ubuf[2],ubuf[3],ubuf[4],ubuf[5],
        ubuf[6],ubuf[7],ubuf[8],ubuf[9],ubuf[10],ubuf[11],
        ubuf[12],ubuf[13],ubuf[14],ubuf[15]);
    std::string token(uuidBuf);

    pm_.updateToken(username, token);
    pm_.updateStatus(username, "online");

    std::cout << "[AuthUDP] Authentification réussie pour : " << username << "\n";
    return buildAuthAck(seq, token);
}

std::string AuthUDPHandler::processLogoutReq(const std::string& username,
                                               const std::string& token,
                                               uint32_t seq) {
    if (pm_.validateToken(username, token)) {
        pm_.updateToken(username, "");
        pm_.updateStatus(username, "offline");
        std::cout << "[AuthUDP] Déconnexion : " << username << "\n";
    }
    return buildLogoutAck(seq);
}

// ================================================================
//  Constructeurs de messages
// ================================================================
std::string AuthUDPHandler::buildAuthAck(uint32_t seq, const std::string& token) {
    Json j = Json::object();
    j["type"]  = Json(std::string(MSG_AUTH_ACK));
    j["seq"]   = Json((int)seq);
    j["token"] = Json(token);
    j["message"] = Json(std::string("ok"));
    return j.dump();
}

std::string AuthUDPHandler::buildAuthNak(uint32_t seq, const std::string& msg) {
    Json j = Json::object();
    j["type"]    = Json(std::string(MSG_AUTH_NAK));
    j["seq"]     = Json((int)seq);
    j["message"] = Json(msg);
    return j.dump();
}

std::string AuthUDPHandler::buildLogoutAck(uint32_t seq) {
    Json j = Json::object();
    j["type"] = Json(std::string(MSG_LOGOUT_ACK));
    j["seq"]  = Json((int)seq);
    return j.dump();
}

// ================================================================
//  Helpers
// ================================================================
std::string AuthUDPHandler::addrKey(const struct sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, (const void*)&addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}
