// =============================================================
//  NetworkProvider.cpp
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "NetworkProvider.h"
#include "../common/json.h"
#include "../common/sha256.h"
#include <cstring>
#include <iostream>
#include <sstream>

// ================================================================
//  Configuration
// ================================================================
void NetworkProvider::setServer(const std::string& ip, int udpPort, int tcpPort) {
    serverIp_ = ip;
    udpPort_  = udpPort;
    tcpPort_  = tcpPort;
}

// ================================================================
//  UDP stop-and-wait
// ================================================================
int NetworkProvider::openUdpSocket() {
    int fd = platform_socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[UDP] socket() erreur\n";
        return -1;
    }
    platform_set_recv_timeout(fd, UDP_TIMEOUT_SEC);
    return fd;
}

std::string NetworkProvider::udpSendRecv(int fd, const std::string& msg,
                                          const struct sockaddr_in& addr) {
    if (sendto(TO_SOCKET(fd), msg.c_str(), (int)msg.size(), 0,
               (const struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDP] sendto() erreur\n";
        return "";
    }
    char buf[UDP_MAX_PKT_SIZE];
    int n = (int)recvfrom(TO_SOCKET(fd), buf, (int)(sizeof(buf) - 1), 0, nullptr, nullptr);
    if (n <= 0) return ""; // timeout ou erreur
    buf[n] = '\0';
    return std::string(buf, n);
}

std::string NetworkProvider::authenticate(const std::string& username,
                                            const std::string& passwordHash) {
    int fd = openUdpSocket();
    if (fd < 0) return "";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)udpPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &addr.sin_addr);

    static uint32_t seqCounter = 0;
    uint32_t seq = ++seqCounter;

    // Construire AUTH_REQ
    Json req = Json::object();
    req["type"]          = Json(std::string(MSG_AUTH_REQ));
    req["seq"]           = Json((int)seq);
    req["username"]      = Json(username);
    req["password_hash"] = Json(passwordHash);
    std::string reqStr = req.dump();

    std::string token;
    for (int attempt = 0; attempt < UDP_MAX_RETRIES; attempt++) {
        std::string resp = udpSendRecv(fd, reqStr, addr);
        if (resp.empty()) {
            std::cout << "[UDP] Timeout, tentative " << (attempt+1) << "/" << UDP_MAX_RETRIES << "\n";
            continue;
        }
        Json jresp;
        try { jresp = Json::parse(resp); } catch (...) { continue; }

        if (!jresp.contains("type")) continue;
        std::string type = jresp.at("type").get_string();

        if (type == MSG_AUTH_ACK && jresp.contains("token")) {
            token = jresp.at("token").get_string();
            std::cout << "[UDP] Authentification réussie\n";
            break;
        } else if (type == MSG_AUTH_NAK) {
            std::string msg = jresp.contains("message") ? jresp.at("message").get_string() : "";
            std::cerr << "[UDP] Authentification refusée : " << msg << "\n";
            break;
        }
    }

    socket_close(fd);
    token_ = token;
    return token;
}

bool NetworkProvider::logout(const std::string& username) {
    if (token_.empty()) return true;
    int fd = openUdpSocket();
    if (fd < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)udpPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &addr.sin_addr);

    static uint32_t seqCounter = 1000;
    uint32_t seq = ++seqCounter;

    Json req = Json::object();
    req["type"]     = Json(std::string(MSG_LOGOUT_REQ));
    req["seq"]      = Json((int)seq);
    req["username"] = Json(username);
    req["token"]    = Json(token_);

    for (int attempt = 0; attempt < UDP_MAX_RETRIES; attempt++) {
        std::string resp = udpSendRecv(fd, req.dump(), addr);
        if (resp.empty()) continue;
        Json jresp;
        try { jresp = Json::parse(resp); } catch (...) { continue; }
        if (jresp.contains("type") && jresp.at("type").get_string() == MSG_LOGOUT_ACK) {
            token_ = "";
            socket_close(fd);
            return true;
        }
    }
    socket_close(fd);
    return false;
}

// ================================================================
//  Client HTTP/1.1 manuel
// ================================================================
int NetworkProvider::openTcpConnection() {
    int fd = platform_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { std::cerr << "[TCP] socket() erreur\n"; return -1; }

    platform_set_recv_timeout(fd, 10);
    platform_set_send_timeout(fd, 10);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)tcpPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &addr.sin_addr);

    if (connect(TO_SOCKET(fd), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[TCP] connect() erreur\n";
        socket_close(fd); return -1;
    }
    return fd;
}

HttpResult NetworkProvider::sendHttp(const std::string& method,
                                      const std::string& path,
                                      const std::string& body,
                                      const std::string& contentType) {
    int fd = openTcpConnection();
    if (fd < 0) return {0, "Connexion impossible"};

    // Construire la requête HTTP/1.1
    std::ostringstream oss;
    oss << method << " " << path << " HTTP/1.1\r\n";
    oss << "Host: " << serverIp_ << "\r\n";
    if (!token_.empty())
        oss << "Authorization: Bearer " << token_ << "\r\n";
    if (!body.empty()) {
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
    }
    oss << "Connection: close\r\n\r\n";
    if (!body.empty()) oss << body;

    std::string reqStr = oss.str();
    if (send(TO_SOCKET(fd), reqStr.c_str(), (int)reqStr.size(), 0) < 0) {
        std::cerr << "[TCP] send() erreur\n";
        socket_close(fd); return {0, "Erreur envoi"};
    }

    // Lire la réponse complète
    std::string raw;
    char buf[4096];
    int n;
    while ((n = recv(TO_SOCKET(fd), buf, (int)sizeof(buf), 0)) > 0)
        raw.append(buf, n);
    socket_close(fd);

    return parseHttpResponse(raw);
}

HttpResult NetworkProvider::parseHttpResponse(const std::string& raw) {
    HttpResult result;
    if (raw.empty()) return result;

    // Ligne de statut
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos) return result;
    std::string statusLine = raw.substr(0, lineEnd);

    // HTTP/1.1 200 OK
    size_t sp1 = statusLine.find(' ');
    if (sp1 == std::string::npos) return result;
    size_t sp2 = statusLine.find(' ', sp1 + 1);
    std::string codeStr = statusLine.substr(sp1 + 1, sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1);
    try { result.statusCode = std::stoi(codeStr); } catch (...) { return result; }

    // Corps : tout ce qui suit le double CRLF
    size_t bodyStart = raw.find("\r\n\r\n");
    if (bodyStart != std::string::npos)
        result.body = raw.substr(bodyStart + 4);

    return result;
}

// ================================================================
//  API REST publique
// ================================================================
HttpResult NetworkProvider::get(const std::string& path) {
    return sendHttp("GET", path, "", "");
}

HttpResult NetworkProvider::post(const std::string& path, const std::string& body) {
    return sendHttp("POST", path, body, "application/json");
}

HttpResult NetworkProvider::put(const std::string& path, const std::string& body,
                                  const std::string& contentType) {
    return sendHttp("PUT", path, body, contentType);
}

HttpResult NetworkProvider::del(const std::string& path) {
    return sendHttp("DELETE", path, "", "");
}

HttpResult NetworkProvider::uploadFile(const std::string& dirId,
                                        const std::string& name,
                                        const std::string& data) {
    return put("/files/" + dirId + "/" + name, data, "application/octet-stream");
}

HttpResult NetworkProvider::downloadFile(const std::string& dirId,
                                          const std::string& name) {
    return get("/files/" + dirId + "/" + name);
}
