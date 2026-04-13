// =============================================================
//  RESTServer.cpp  –  API HTTP/1.1 manuelle sur socket TCP
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "RESTServer.h"
#include "../common/json.h"
#include "../common/FileMetadata.h"
#include "../common/sha256.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

// ================================================================
//  HttpResponse helpers
// ================================================================
HttpResponse HttpResponse::ok(const std::string& body) {
    HttpResponse r; r.status = 200; r.statusMsg = "OK"; r.body = body; return r;
}
HttpResponse HttpResponse::error(int code, const std::string& msg) {
    HttpResponse r;
    r.status = code;
    r.statusMsg = statusText(code);
    Json j = Json::object(); j["error"] = Json(msg);
    r.body = j.dump();
    return r;
}
HttpResponse HttpResponse::notFound()     { return error(404, "Not Found"); }
HttpResponse HttpResponse::unauthorized() { return error(401, "Non autorisé"); }
HttpResponse HttpResponse::forbidden()    { return error(403, "Accès refusé"); }

// ================================================================
//  Constructeur / Destructeur
// ================================================================
RESTServer::RESTServer(PersistenceManager& pm) : pm_(pm) {}
RESTServer::~RESTServer() { stop(); }

// ================================================================
//  Démarrage
// ================================================================
bool RESTServer::start(int port) {
    listenFd_ = platform_socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) { std::cerr << "[REST] socket() erreur\n"; return false; }

    int opt = 1;
    setsockopt(TO_SOCKET(listenFd_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(TO_SOCKET(listenFd_), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[REST] bind() erreur\n";
        socket_close(listenFd_); listenFd_ = -1; return false;
    }
    if (listen(TO_SOCKET(listenFd_), 32) < 0) {
        std::cerr << "[REST] listen() erreur\n";
        socket_close(listenFd_); listenFd_ = -1; return false;
    }
    running_ = true;
    acceptThread_ = std::thread(&RESTServer::acceptLoop, this);
    std::cout << "[INFO] Serveur REST en écoute sur le port " << port << "\n";
    return true;
}

void RESTServer::stop() {
    running_ = false;
    if (listenFd_ >= 0) {
        shutdown(TO_SOCKET(listenFd_), SHUT_RDWR);
        socket_close(listenFd_);
        listenFd_ = -1;
    }
    if (acceptThread_.joinable()) acceptThread_.join();
}

void RESTServer::acceptLoop() {
    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int clientFd = (int)accept(TO_SOCKET(listenFd_), (struct sockaddr*)&clientAddr, &len);
        if (clientFd < 0) { if (running_) std::cerr << "[REST] accept() erreur\n"; break; }
        // Chaque connexion est traitée dans un thread dédié
        std::thread([this, clientFd](){ handleConnection(clientFd); }).detach();
    }
}

// ================================================================
//  Traitement d'une connexion
// ================================================================
void RESTServer::handleConnection(int fd) {
    HttpRequest req;
    if (!parseRequest(fd, req)) { socket_close(fd); return; }
    bool ok = extractAuth(req);
    HttpResponse res = route(req);
    sendResponse(fd, res);
    socket_close(fd);
    (void)ok;
}

// ================================================================
//  Parsing HTTP/1.1
// ================================================================
bool RESTServer::parseRequest(int fd, HttpRequest& req) {
    // Lire tout ce que le client envoie (max 8 MB pour les fichiers)
    std::string raw;
    char buf[4096];
    int n;
    // Lire jusqu'au double CRLF (fin des en-têtes)
    while (raw.find("\r\n\r\n") == std::string::npos) {
        n = recv(TO_SOCKET(fd), buf, (int)(sizeof(buf) - 1), 0);
        if (n <= 0) return false;
        buf[n] = '\0';
        raw.append(buf, n);
        if (raw.size() > 1024 * 1024 * 8) return false; // sécurité
    }

    size_t headerEnd = raw.find("\r\n\r\n");
    std::string headerPart = raw.substr(0, headerEnd);
    std::string bodyPart   = raw.substr(headerEnd + 4);

    // Ligne de requête
    std::istringstream iss(headerPart);
    std::string version;
    iss >> req.method >> req.path >> version;

    // En-têtes
    std::string line;
    std::getline(iss, line); // fin de la première ligne
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // Trim
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        while (!key.empty() && key.back()  == ' ') key.pop_back();
        // Lowercase key
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        req.headers[key] = val;
    }

    // Content-Length → lire le corps complet
    int contentLength = 0;
    if (req.headers.count("content-length"))
        contentLength = std::stoi(req.headers["content-length"]);

    req.body = bodyPart;
    while ((int)req.body.size() < contentLength) {
        int toRead = std::min((int)sizeof(buf)-1, contentLength - (int)req.body.size());
        n = recv(TO_SOCKET(fd), buf, toRead, 0);
        if (n <= 0) break;
        req.body.append(buf, n);
    }

    // Décomposer le chemin
    std::string p = req.path;
    if (p.empty() || p[0] != '/') return false;
    p = p.substr(1); // retirer le '/' initial
    std::istringstream ps(p);
    std::string part;
    while (std::getline(ps, part, '/'))
        if (!part.empty()) req.pathParts.push_back(part);

    return true;
}

void RESTServer::sendResponse(int fd, const HttpResponse& res) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status << " " << res.statusMsg << "\r\n";
    oss << "Content-Type: " << res.contentType << "\r\n";
    oss << "Content-Length: " << res.body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << res.body;
    std::string msg = oss.str();
    send(TO_SOCKET(fd), msg.c_str(), (int)msg.size(), 0);
}

std::string HttpResponse::statusText(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    default:  return "Internal Server Error";
    }
}

// ================================================================
//  Authentification
// ================================================================
bool RESTServer::extractAuth(HttpRequest& req) {
    if (!req.headers.count("authorization")) return false;
    std::string auth = req.headers["authorization"];
    const std::string prefix = "Bearer ";
    if (auth.substr(0, prefix.size()) != prefix) return false;
    req.bearerToken = auth.substr(prefix.size());
    return true;
}

// Chercher quel utilisateur possède ce token
static std::string findUserByToken(PersistenceManager& pm, const std::string& token) {
    for (const auto& c : pm.getAllClients())
        if (c.session_token == token && !token.empty())
            return c.username;
    return "";
}

// ================================================================
//  Routage
// ================================================================
HttpResponse RESTServer::route(HttpRequest& req) {
    const auto& pp = req.pathParts;
    const std::string& m = req.method;

    // POST /auth/login  (pas besoin de token)
    if (m == "POST" && pp.size() == 2 && pp[0] == "auth" && pp[1] == "login")
        return handleAuthLogin(req);

    // Tous les autres endpoints nécessitent un token valide
    req.authUser = findUserByToken(pm_, req.bearerToken);
    if (req.authUser.empty() && !(pp.size() == 2 && pp[0] == "auth"))
        return HttpResponse::unauthorized();

    // POST /auth/logout
    if (m == "POST" && pp.size() == 2 && pp[0] == "auth" && pp[1] == "logout")
        return handleAuthLogout(req);

    // GET /users
    if (m == "GET" && pp.size() == 1 && pp[0] == "users")
        return handleGetUsers(req);

    // GET /directories
    if (m == "GET" && pp.size() == 1 && pp[0] == "directories")
        return handleGetDirectories(req);

    // POST /directories
    if (m == "POST" && pp.size() == 1 && pp[0] == "directories")
        return handlePostDirectories(req);

    // POST /directories/{id}/invitations
    if (m == "POST" && pp.size() == 3 && pp[0] == "directories" && pp[2] == "invitations")
        return handlePostInviteToDir(req);

    // DELETE /directories/{id}/members/{uid}
    if (m == "DELETE" && pp.size() == 4 && pp[0] == "directories" && pp[2] == "members")
        return handleDeleteDirMember(req);

    // PUT /directories/{id}/admin
    if (m == "PUT" && pp.size() == 3 && pp[0] == "directories" && pp[2] == "admin")
        return handlePutDirAdmin(req);

    // GET /invitations
    if (m == "GET" && pp.size() == 1 && pp[0] == "invitations")
        return handleGetInvitations(req);

    // POST /invitations/{id}/accept
    if (m == "POST" && pp.size() == 3 && pp[0] == "invitations" && pp[2] == "accept")
        return handlePostAcceptInvitation(req);

    // POST /sync/{dir_id}
    if (m == "POST" && pp.size() == 2 && pp[0] == "sync")
        return handlePostSync(req);

    // PUT /files/{dir_id}/{name}
    if (m == "PUT" && pp.size() == 3 && pp[0] == "files")
        return handlePutFile(req);

    // GET /files/{dir_id}/{name}
    if (m == "GET" && pp.size() == 3 && pp[0] == "files")
        return handleGetFile(req);

    // DELETE /files/{dir_id}/{name}
    if (m == "DELETE" && pp.size() == 3 && pp[0] == "files")
        return handleDeleteFile(req);

    return HttpResponse::notFound();
}

// ================================================================
//  Handlers
// ================================================================

HttpResponse RESTServer::handleAuthLogin(HttpRequest& req) {
    Json body;
    try { body = Json::parse(req.body); } catch (...) { return HttpResponse::error(400, "JSON invalide"); }
    if (!body.contains("user") || !body.contains("token"))
        return HttpResponse::error(400, "Champs manquants");

    std::string user  = body.at("user").get_string();
    std::string token = body.at("token").get_string();

    if (!pm_.validateToken(user, token))
        return HttpResponse::unauthorized();

    pm_.updateStatus(user, "online");
    Json resp = Json::object();
    resp["message"] = Json(std::string("Session REST ouverte"));
    resp["username"] = Json(user);
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handleAuthLogout(HttpRequest& req) {
    pm_.updateToken(req.authUser, "");
    pm_.updateStatus(req.authUser, "offline");
    Json resp = Json::object();
    resp["message"] = Json(std::string("Déconnecté"));
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handleGetUsers(HttpRequest& req) {
    (void)req;
    Json arr = Json::array();
    for (const auto& c : pm_.getAllClients()) {
        Json u = Json::object();
        u["username"] = Json(c.username);
        u["status"]   = Json(c.status);
        arr.push_back(u);
    }
    Json resp = Json::object();
    resp["users"] = arr;
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handleGetDirectories(HttpRequest& req) {
    Json arr = Json::array();
    for (const auto& d : pm_.getDirectoriesForUser(req.authUser)) {
        Json dj = Json::object();
        dj["id"]    = Json(d.id);
        dj["name"]  = Json(d.name);
        dj["admin"] = Json(d.admin);
        Json members = Json::array();
        for (const auto& m : d.members) members.push_back(Json(m));
        dj["members"] = members;
        arr.push_back(dj);
    }
    Json resp = Json::object();
    resp["directories"] = arr;
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handlePostDirectories(HttpRequest& req) {
    Json body;
    try { body = Json::parse(req.body); } catch (...) { return HttpResponse::error(400, "JSON invalide"); }
    if (!body.contains("name")) return HttpResponse::error(400, "Champ 'name' manquant");

    std::string name = body.at("name").get_string();
    if (name.empty()) return HttpResponse::error(400, "Nom vide");

    Directory d = pm_.createDirectory(name, req.authUser);
    Json resp = Json::object();
    resp["id"]   = Json(d.id);
    resp["name"] = Json(d.name);
    HttpResponse r = HttpResponse::ok(resp.dump());
    r.status = 201; r.statusMsg = "Created";
    return r;
}

HttpResponse RESTServer::handlePostInviteToDir(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    if (!pm_.isAdmin(dirId, req.authUser)) return HttpResponse::forbidden();

    Json body;
    try { body = Json::parse(req.body); } catch (...) { return HttpResponse::error(400, "JSON invalide"); }
    if (!body.contains("user_id")) return HttpResponse::error(400, "Champ 'user_id' manquant");
    std::string toUser = body.at("user_id").get_string();

    if (!pm_.clientExists(toUser)) return HttpResponse::error(404, "Utilisateur inconnu");
    if (pm_.isMember(dirId, toUser)) return HttpResponse::error(409, "Déjà membre");

    Invitation inv = pm_.createInvitation(dirId, req.authUser, toUser);
    Json resp = Json::object();
    resp["invitation_id"] = Json(inv.id);
    HttpResponse r = HttpResponse::ok(resp.dump());
    r.status = 201; r.statusMsg = "Created";
    return r;
}

HttpResponse RESTServer::handleDeleteDirMember(HttpRequest& req) {
    std::string dirId  = req.pathParts[1];
    std::string userId = req.pathParts[3];
    if (!pm_.isAdmin(dirId, req.authUser)) return HttpResponse::forbidden();
    if (!pm_.isMember(dirId, userId)) return HttpResponse::notFound();
    pm_.removeMember(dirId, userId);
    Json resp = Json::object();
    resp["message"] = Json(std::string("Membre retiré"));
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handlePutDirAdmin(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    if (!pm_.isAdmin(dirId, req.authUser)) return HttpResponse::forbidden();

    Json body;
    try { body = Json::parse(req.body); } catch (...) { return HttpResponse::error(400, "JSON invalide"); }
    if (!body.contains("user_id")) return HttpResponse::error(400, "Champ 'user_id' manquant");
    std::string newAdmin = body.at("user_id").get_string();

    if (!pm_.isMember(dirId, newAdmin)) return HttpResponse::error(400, "L'utilisateur doit être membre");
    pm_.transferAdmin(dirId, newAdmin);
    Json resp = Json::object();
    resp["message"] = Json(std::string("Administrateur transféré"));
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handleGetInvitations(HttpRequest& req) {
    Json arr = Json::array();
    for (const auto& inv : pm_.getPendingInvitations(req.authUser)) {
        Json j = Json::object();
        j["id"]           = Json(inv.id);
        j["directory_id"] = Json(inv.directory_id);
        j["from_user"]    = Json(inv.from_user);
        j["status"]       = Json(inv.status);
        arr.push_back(j);
    }
    Json resp = Json::object();
    resp["invitations"] = arr;
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handlePostAcceptInvitation(HttpRequest& req) {
    std::string invId = req.pathParts[1];

    // Lire l'invitation AVANT de la marquer acceptée pour récupérer le dir_id
    Invitation inv = pm_.getInvitation(invId);
    if (inv.id.empty() || inv.to_user != req.authUser)
        return HttpResponse::error(404, "Invitation introuvable");
    if (inv.status != "pending")
        return HttpResponse::error(409, "Invitation déjà traitée");

    // Marquer l'invitation comme acceptée
    pm_.acceptInvitation(invId, req.authUser);

    // Ajouter l'utilisateur au répertoire partagé
    pm_.addMember(inv.directory_id, req.authUser);

    Json resp = Json::object();
    resp["message"]      = Json(std::string("Invitation acceptée"));
    resp["directory_id"] = Json(inv.directory_id);
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handlePostSync(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    if (!pm_.isMember(dirId, req.authUser)) return HttpResponse::forbidden();

    // Désérialiser la liste de FileMetadata locale envoyée par le client
    Json body;
    try { body = Json::parse(req.body); } catch (...) { return HttpResponse::error(400, "JSON invalide"); }

    std::vector<FileMetadata> clientFiles;
    if (body.is_array()) {
        for (const auto& e : body.get_array())
            clientFiles.push_back(FileMetadata::fromJson(e));
    }

    // Récupérer l'état serveur
    std::vector<FileMetadata> serverFiles = pm_.getFilesMetadata(dirId);

    // Calculer les différences
    Json toDownload = Json::array(); // fichiers que le client doit télécharger
    Json toUpload   = Json::array(); // fichiers que le client doit uploader

    // Index des fichiers client par nom
    std::map<std::string, FileMetadata> clientIdx;
    for (const auto& f : clientFiles) clientIdx[f.name] = f;

    // Index des fichiers serveur par nom
    std::map<std::string, FileMetadata> serverIdx;
    for (const auto& f : serverFiles) serverIdx[f.name] = f;

    // Fichiers présents sur le serveur : vérifier si le client doit télécharger
    for (const auto& sf : serverFiles) {
        if (sf.deleted) continue;
        auto it = clientIdx.find(sf.name);
        if (it == clientIdx.end() || it->second.hash != sf.hash)
            toDownload.push_back(sf.toJson());
    }

    // Fichiers présents chez le client : vérifier si le serveur doit recevoir
    for (const auto& cf : clientFiles) {
        if (cf.deleted) continue;
        auto it = serverIdx.find(cf.name);
        if (it == serverIdx.end() || it->second.hash != cf.hash)
            toUpload.push_back(Json(cf.name));
    }

    Json resp = Json::object();
    resp["to_download"] = toDownload;
    resp["to_upload"]   = toUpload;
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handlePutFile(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    std::string name  = req.pathParts[2];
    if (!pm_.isMember(dirId, req.authUser)) return HttpResponse::forbidden();

    std::string filePath = pm_.getFilePath(dirId, name);
    std::ofstream f(filePath, std::ios::binary);
    if (!f) return HttpResponse::error(500, "Impossible d'écrire le fichier");
    f.write(req.body.c_str(), (std::streamsize)req.body.size());
    f.close();

    // Mettre à jour les métadonnées
    FileMetadata meta;
    meta.name    = name;
    meta.dir_id  = dirId;
    meta.hash    = SHA256::hash(reinterpret_cast<const unsigned char*>(req.body.c_str()),
                                req.body.size());
    meta.size    = (long long)req.body.size();
    meta.mtime   = (long long)time(nullptr);
    meta.deleted = false;
    pm_.saveFileMetadata(meta);

    Json resp = Json::object();
    resp["message"] = Json(std::string("Fichier uploadé"));
    resp["hash"]    = Json(meta.hash);
    return HttpResponse::ok(resp.dump());
}

HttpResponse RESTServer::handleGetFile(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    std::string name  = req.pathParts[2];
    if (!pm_.isMember(dirId, req.authUser)) return HttpResponse::forbidden();
    if (!pm_.fileExists(dirId, name)) return HttpResponse::notFound();

    std::string filePath = pm_.getFilePath(dirId, name);
    std::ifstream f(filePath, std::ios::binary);
    if (!f) return HttpResponse::error(500, "Impossible de lire le fichier");
    std::ostringstream ss;
    ss << f.rdbuf();

    HttpResponse res;
    res.status      = 200;
    res.statusMsg   = "OK";
    res.contentType = "application/octet-stream";
    res.body        = ss.str();
    return res;
}

HttpResponse RESTServer::handleDeleteFile(HttpRequest& req) {
    std::string dirId = req.pathParts[1];
    std::string name  = req.pathParts[2];
    if (!pm_.isMember(dirId, req.authUser)) return HttpResponse::forbidden();

    std::string filePath = pm_.getFilePath(dirId, name);
    (void)std::remove(filePath.c_str());
    pm_.deleteFileMetadata(dirId, name);

    Json resp = Json::object();
    resp["message"] = Json(std::string("Fichier supprimé"));
    return HttpResponse::ok(resp.dump());
}
