#pragma once
// =============================================================
//  RESTServer.h  –  API HTTP/1.1 manuelle sur socket TCP
//  IFT585 – TP4
//
//  Endpoints implémentés (cf. rapport section 6) :
//    POST   /auth/login
//    POST   /auth/logout
//    GET    /users
//    GET    /directories
//    POST   /directories
//    POST   /directories/{id}/invitations
//    DELETE /directories/{id}/members/{uid}
//    PUT    /directories/{id}/admin
//    GET    /invitations
//    POST   /invitations/{id}/accept
//    POST   /sync/{dir_id}
//    PUT    /files/{dir_id}/{name}
//    GET    /files/{dir_id}/{name}
//    DELETE /files/{dir_id}/{name}
// =============================================================
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include <thread>
#include "PersistenceManager.h"

struct HttpRequest {
    std::string              method;
    std::string              path;
    std::map<std::string,std::string> headers;
    std::string              body;
    std::vector<std::string> pathParts; // chemin découpé par '/'
    std::string              bearerToken;
    std::string              authUser;  // rempli après validation du token
};

struct HttpResponse {
    int         status  = 200;
    std::string statusMsg = "OK";
    std::string contentType = "application/json";
    std::string body;

    static HttpResponse ok(const std::string& body);
    static HttpResponse error(int code, const std::string& msg);
    static HttpResponse notFound();
    static HttpResponse unauthorized();
    static HttpResponse forbidden();
    static std::string  statusText(int code);
};

class RESTServer {
public:
    explicit RESTServer(PersistenceManager& pm);
    ~RESTServer();

    bool start(int port = 80);
    void stop();
    bool isRunning() const { return running_; }

    // Traite une connexion TCP (appelé par le thread pool de ServerCore)
    void handleConnection(int clientFd);

private:
    PersistenceManager& pm_;
    int                 listenFd_ = -1;
    std::atomic<bool>   running_{false};
    std::thread         acceptThread_;

    void acceptLoop();

    // ---- Parsing HTTP ----
    bool       parseRequest(int fd, HttpRequest& req);
    void       sendResponse(int fd, const HttpResponse& res);

    // ---- Authentification ----
    bool extractAuth(HttpRequest& req);

    // ---- Routage ----
    HttpResponse route(HttpRequest& req);

    // ---- Handlers ----
    HttpResponse handleAuthLogin(HttpRequest& req);
    HttpResponse handleAuthLogout(HttpRequest& req);
    HttpResponse handleGetUsers(HttpRequest& req);
    HttpResponse handleGetDirectories(HttpRequest& req);
    HttpResponse handlePostDirectories(HttpRequest& req);
    HttpResponse handlePostInviteToDir(HttpRequest& req);
    HttpResponse handleDeleteDirMember(HttpRequest& req);
    HttpResponse handlePutDirAdmin(HttpRequest& req);
    HttpResponse handleGetInvitations(HttpRequest& req);
    HttpResponse handlePostAcceptInvitation(HttpRequest& req);
    HttpResponse handlePostDeclineInvitation(HttpRequest& req);
    HttpResponse handlePostSync(HttpRequest& req);
    HttpResponse handlePutFile(HttpRequest& req);
    HttpResponse handleGetFile(HttpRequest& req);
    HttpResponse handleDeleteFile(HttpRequest& req);
};
