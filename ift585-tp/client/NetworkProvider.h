#pragma once
// =============================================================
//  NetworkProvider.h  –  Transport UDP (stop-and-wait) + REST
//  IFT585 – TP4
//
//  Deux rôles :
//    1) UDP stop-and-wait  → authentification avec le serveur
//    2) Client HTTP/1.1    → toutes les requêtes REST
// =============================================================
#include <string>
#include <map>
#include "../common/UDPProtocol.h"

struct HttpResult {
    int         statusCode = 0;
    std::string body;
    bool        ok() const { return statusCode >= 200 && statusCode < 300; }
};

class NetworkProvider {
public:
    NetworkProvider() = default;

    // ---- Configuration ----
    void setServer(const std::string& ip, int udpPort = UDP_AUTH_PORT,
                   int tcpPort = TCP_REST_PORT);
    void setToken(const std::string& token) { token_ = token; }
    const std::string& getToken() const     { return token_; }

    // ---- UDP stop-and-wait : authentification ----
    // Retourne le token de session ou "" en cas d'échec
    std::string authenticate(const std::string& username,
                              const std::string& passwordHash);

    // Notifie le serveur de la déconnexion via UDP
    bool logout(const std::string& username);

    // ---- Client HTTP/1.1 manuel ----
    HttpResult get(const std::string& path);
    HttpResult post(const std::string& path, const std::string& body);
    HttpResult put(const std::string& path, const std::string& body,
                   const std::string& contentType = "application/json");
    HttpResult del(const std::string& path);

    // ---- Upload / Download binaire ----
    HttpResult uploadFile(const std::string& dirId,
                           const std::string& name,
                           const std::string& data);
    HttpResult downloadFile(const std::string& dirId,
                             const std::string& name);

private:
    std::string serverIp_;
    int         udpPort_  = UDP_AUTH_PORT;
    int         tcpPort_  = TCP_REST_PORT;
    std::string token_;    // SessionToken Bearer

    // ---- Helpers UDP ----
    int         openUdpSocket();
    std::string udpSendRecv(int sockfd, const std::string& msg,
                             const struct sockaddr_in& addr);

    // ---- Helpers TCP/HTTP ----
    int         openTcpConnection();
    HttpResult  sendHttp(const std::string& method,
                          const std::string& path,
                          const std::string& body,
                          const std::string& contentType);
    static HttpResult parseHttpResponse(const std::string& raw);
};
