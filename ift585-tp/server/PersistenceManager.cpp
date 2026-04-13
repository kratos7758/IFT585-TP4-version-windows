// =============================================================
//  PersistenceManager.cpp
//  IFT585 – TP4
// =============================================================
#include "../common/platform.h"
#include "PersistenceManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <ctime>

// ================================================================
//  Constructeur
// ================================================================
PersistenceManager::PersistenceManager(const std::string& dataDir)
    : dataDir_(dataDir)
{
    ensureDir(dataDir_);
    ensureDir(dataDir_ + "/files");

    // Initialiser les fichiers JSON s'ils n'existent pas
    auto initJson = [&](const std::string& path, const Json& init) {
        std::ifstream f(path);
        if (!f.good()) writeJson(path, init);
    };
    Json emptyClients = Json::object();
    emptyClients["clients"] = Json::array();
    initJson(dataDir_ + "/clients.json", emptyClients);

    Json emptyDirs = Json::object();
    emptyDirs["directories"] = Json::array();
    initJson(dataDir_ + "/directories.json", emptyDirs);

    Json emptyInvs = Json::object();
    emptyInvs["invitations"] = Json::array();
    initJson(dataDir_ + "/invitations.json", emptyInvs);
}

// ================================================================
//  Helpers I/O
// ================================================================
Json PersistenceManager::readJson(const std::string& path) {
    std::ifstream f(path);
    if (!f) return Json::object();
    std::ostringstream ss;
    ss << f.rdbuf();
    return Json::parse(ss.str());
}

void PersistenceManager::writeJson(const std::string& path, const Json& j) {
    std::ofstream f(path);
    if (!f) { std::cerr << "[PersistenceManager] Impossible d'écrire : " << path << "\n"; return; }
    f << j.dump(2);
}

Json PersistenceManager::readClients()     { return readJson(dataDir_ + "/clients.json"); }
Json PersistenceManager::readDirectories() { return readJson(dataDir_ + "/directories.json"); }
Json PersistenceManager::readInvitations() { return readJson(dataDir_ + "/invitations.json"); }

void PersistenceManager::writeClients(const Json& j)     { writeJson(dataDir_ + "/clients.json", j); }
void PersistenceManager::writeDirectories(const Json& j) { writeJson(dataDir_ + "/directories.json", j); }
void PersistenceManager::writeInvitations(const Json& j) { writeJson(dataDir_ + "/invitations.json", j); }

void PersistenceManager::ensureDir(const std::string& path) {
    (void)platform_mkdir(path.c_str());
}

std::string PersistenceManager::makeUUID() {
    unsigned char buf[16];
    if (!platform_random_bytes(buf, 16)) {
        srand((unsigned)time(nullptr));
        for (auto& b : buf) b = (unsigned char)(rand() & 0xff);
    }

    // RFC 4122 v4
    buf[6] = (buf[6] & 0x0f) | 0x40;
    buf[8] = (buf[8] & 0x3f) | 0x80;

    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        buf[0],buf[1],buf[2],buf[3],
        buf[4],buf[5],buf[6],buf[7],
        buf[8],buf[9],buf[10],buf[11],
        buf[12],buf[13],buf[14],buf[15]);
    return std::string(uuid);
}

// ================================================================
//  Clients
// ================================================================
static ClientProfile clientFromJson(const Json& j) {
    ClientProfile c;
    if (j.contains("username"))      c.username      = j.at("username").get_string();
    if (j.contains("password_hash")) c.password_hash = j.at("password_hash").get_string();
    if (j.contains("status"))        c.status        = j.at("status").get_string();
    if (j.contains("session_token")) c.session_token = j.at("session_token").get_string();
    if (j.contains("token_expiry"))  c.token_expiry  = j.at("token_expiry").get_ll();
    return c;
}

static Json clientToJson(const ClientProfile& c) {
    Json j = Json::object();
    j["username"]      = Json(c.username);
    j["password_hash"] = Json(c.password_hash);
    j["status"]        = Json(c.status);
    j["session_token"] = Json(c.session_token);
    j["token_expiry"]  = Json(c.token_expiry);
    return j;
}

bool PersistenceManager::clientExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    const auto& arr = root.at("clients").get_array();
    for (const auto& e : arr)
        if (e.at("username").get_string() == username) return true;
    return false;
}

ClientProfile PersistenceManager::getClient(const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    for (const auto& e : root.at("clients").get_array())
        if (e.at("username").get_string() == username)
            return clientFromJson(e);
    return {};
}

std::vector<ClientProfile> PersistenceManager::getAllClients() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<ClientProfile> result;
    Json root = readClients();
    for (const auto& e : root.at("clients").get_array())
        result.push_back(clientFromJson(e));
    return result;
}

void PersistenceManager::saveClient(const ClientProfile& c) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    auto& arr = root["clients"].get_array();
    for (auto& e : arr) {
        if (e.at("username").get_string() == c.username) {
            e = clientToJson(c);
            writeClients(root);
            return;
        }
    }
    arr.push_back(clientToJson(c));
    writeClients(root);
}

bool PersistenceManager::validateToken(const std::string& username,
                                        const std::string& token) {
    if (token.empty()) return false;
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    for (const auto& e : root.at("clients").get_array())
        if (e.at("username").get_string() == username &&
            e.at("session_token").get_string() == token)
            return true;
    return false;
}

void PersistenceManager::updateToken(const std::string& username,
                                      const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    for (auto& e : root["clients"].get_array()) {
        if (e.at("username").get_string() == username) {
            e["session_token"] = Json(token);
            writeClients(root);
            return;
        }
    }
}

void PersistenceManager::updateStatus(const std::string& username,
                                       const std::string& status) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readClients();
    for (auto& e : root["clients"].get_array()) {
        if (e.at("username").get_string() == username) {
            e["status"] = Json(status);
            writeClients(root);
            return;
        }
    }
}

// ================================================================
//  Répertoires
// ================================================================
static Directory dirFromJson(const Json& j) {
    Directory d;
    if (j.contains("id"))    d.id    = j.at("id").get_string();
    if (j.contains("name"))  d.name  = j.at("name").get_string();
    if (j.contains("admin")) d.admin = j.at("admin").get_string();
    if (j.contains("members")) {
        for (const auto& m : j.at("members").get_array())
            d.members.push_back(m.get_string());
    }
    return d;
}

static Json dirToJson(const Directory& d) {
    Json j = Json::object();
    j["id"]    = Json(d.id);
    j["name"]  = Json(d.name);
    j["admin"] = Json(d.admin);
    Json members = Json::array();
    for (const auto& m : d.members) members.push_back(Json(m));
    j["members"] = members;
    return j;
}

Directory PersistenceManager::createDirectory(const std::string& name,
                                               const std::string& admin) {
    Directory d;
    d.id    = makeUUID();
    d.name  = name;
    d.admin = admin;
    d.members.push_back(admin);
    ensureDir(dataDir_ + "/files/" + d.id);

    std::lock_guard<std::mutex> lock(mu_);
    Json root = readDirectories();
    root["directories"].get_array().push_back(dirToJson(d));
    writeDirectories(root);
    return d;
}

Directory PersistenceManager::getDirectory(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readDirectories();
    for (const auto& e : root.at("directories").get_array())
        if (e.at("id").get_string() == id) return dirFromJson(e);
    return {};
}

std::vector<Directory> PersistenceManager::getDirectoriesForUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Directory> result;
    Json root = readDirectories();
    for (const auto& e : root.at("directories").get_array()) {
        Directory d = dirFromJson(e);
        for (const auto& m : d.members)
            if (m == username) { result.push_back(d); break; }
    }
    return result;
}

void PersistenceManager::saveDirectory(const Directory& d) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readDirectories();
    auto& arr = root["directories"].get_array();
    for (auto& e : arr) {
        if (e.at("id").get_string() == d.id) {
            e = dirToJson(d);
            writeDirectories(root);
            return;
        }
    }
    arr.push_back(dirToJson(d));
    writeDirectories(root);
}

bool PersistenceManager::deleteDirectory(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readDirectories();
    auto& arr = root["directories"].get_array();
    auto it = std::remove_if(arr.begin(), arr.end(),
        [&](const Json& e){ return e.at("id").get_string() == id; });
    if (it == arr.end()) return false;
    arr.erase(it, arr.end());
    writeDirectories(root);
    return true;
}

bool PersistenceManager::isMember(const std::string& dirId,
                                   const std::string& username) {
    Directory d = getDirectory(dirId);
    return std::find(d.members.begin(), d.members.end(), username) != d.members.end();
}

bool PersistenceManager::isAdmin(const std::string& dirId,
                                  const std::string& username) {
    Directory d = getDirectory(dirId);
    return d.admin == username;
}

void PersistenceManager::addMember(const std::string& dirId,
                                    const std::string& username) {
    Directory d = getDirectory(dirId);
    if (std::find(d.members.begin(), d.members.end(), username) == d.members.end())
        d.members.push_back(username);
    saveDirectory(d);
}

void PersistenceManager::removeMember(const std::string& dirId,
                                       const std::string& username) {
    Directory d = getDirectory(dirId);
    d.members.erase(
        std::remove(d.members.begin(), d.members.end(), username),
        d.members.end());
    saveDirectory(d);
}

void PersistenceManager::transferAdmin(const std::string& dirId,
                                        const std::string& newAdmin) {
    Directory d = getDirectory(dirId);
    d.admin = newAdmin;
    saveDirectory(d);
}

// ================================================================
//  Invitations
// ================================================================
static Invitation invFromJson(const Json& j) {
    Invitation inv;
    if (j.contains("id"))           inv.id           = j.at("id").get_string();
    if (j.contains("directory_id")) inv.directory_id = j.at("directory_id").get_string();
    if (j.contains("from_user"))    inv.from_user    = j.at("from_user").get_string();
    if (j.contains("to_user"))      inv.to_user      = j.at("to_user").get_string();
    if (j.contains("status"))       inv.status       = j.at("status").get_string();
    return inv;
}

static Json invToJson(const Invitation& inv) {
    Json j = Json::object();
    j["id"]           = Json(inv.id);
    j["directory_id"] = Json(inv.directory_id);
    j["from_user"]    = Json(inv.from_user);
    j["to_user"]      = Json(inv.to_user);
    j["status"]       = Json(inv.status);
    return j;
}

Invitation PersistenceManager::createInvitation(const std::string& dirId,
                                                  const std::string& from,
                                                  const std::string& to) {
    Invitation inv;
    inv.id           = makeUUID();
    inv.directory_id = dirId;
    inv.from_user    = from;
    inv.to_user      = to;
    inv.status       = "pending";

    std::lock_guard<std::mutex> lock(mu_);
    Json root = readInvitations();
    root["invitations"].get_array().push_back(invToJson(inv));
    writeInvitations(root);
    return inv;
}

std::vector<Invitation> PersistenceManager::getPendingInvitations(const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Invitation> result;
    Json root = readInvitations();
    for (const auto& e : root.at("invitations").get_array()) {
        Invitation inv = invFromJson(e);
        if (inv.to_user == username && inv.status == "pending")
            result.push_back(inv);
    }
    return result;
}

Invitation PersistenceManager::getInvitation(const std::string& invId) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readInvitations();
    for (const auto& e : root.at("invitations").get_array()) {
        Invitation inv = invFromJson(e);
        if (inv.id == invId) return inv;
    }
    return {};
}

bool PersistenceManager::acceptInvitation(const std::string& invId,
                                           const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readInvitations();
    for (auto& e : root["invitations"].get_array()) {
        if (e.at("id").get_string() == invId &&
            e.at("to_user").get_string() == username) {
            e["status"] = Json(std::string("accepted"));
            writeInvitations(root);
            return true;
        }
    }
    return false;
}

bool PersistenceManager::declineInvitation(const std::string& invId,
                                            const std::string& username) {
    std::lock_guard<std::mutex> lock(mu_);
    Json root = readInvitations();
    for (auto& e : root["invitations"].get_array()) {
        if (e.at("id").get_string() == invId &&
            e.at("to_user").get_string() == username) {
            e["status"] = Json(std::string("declined"));
            writeInvitations(root);
            return true;
        }
    }
    return false;
}

// ================================================================
//  Métadonnées de fichiers
// ================================================================
std::vector<FileMetadata> PersistenceManager::getFilesMetadata(const std::string& dirId) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string path = dataDir_ + "/files/" + dirId + "/metadata.json";
    Json root = readJson(path);
    std::vector<FileMetadata> result;
    if (!root.contains("files")) return result;
    for (const auto& e : root.at("files").get_array())
        result.push_back(FileMetadata::fromJson(e));
    return result;
}

void PersistenceManager::saveFileMetadata(const FileMetadata& meta) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string dirPath = dataDir_ + "/files/" + meta.dir_id;
    ensureDir(dirPath);
    std::string path = dirPath + "/metadata.json";
    Json root = readJson(path);
    if (!root.contains("files")) {
        root["files"] = Json::array();
    }
    auto& arr = root["files"].get_array();
    for (auto& e : arr) {
        if (e.at("name").get_string() == meta.name) {
            e = meta.toJson();
            writeJson(path, root);
            return;
        }
    }
    arr.push_back(meta.toJson());
    writeJson(path, root);
}

void PersistenceManager::deleteFileMetadata(const std::string& dirId,
                                              const std::string& name) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string path = dataDir_ + "/files/" + dirId + "/metadata.json";
    Json root = readJson(path);
    if (!root.contains("files")) return;
    auto& arr = root["files"].get_array();
    arr.erase(std::remove_if(arr.begin(), arr.end(),
        [&](const Json& e){ return e.at("name").get_string() == name; }), arr.end());
    writeJson(path, root);
}

std::string PersistenceManager::getFilePath(const std::string& dirId,
                                              const std::string& name) {
    return dataDir_ + "/files/" + dirId + "/" + name;
}

bool PersistenceManager::fileExists(const std::string& dirId,
                                     const std::string& name) {
    std::string path = getFilePath(dirId, name);
    return platform_file_exists(path.c_str());
}
