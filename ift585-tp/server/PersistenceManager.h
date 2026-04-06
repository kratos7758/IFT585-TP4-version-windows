#pragma once
// =============================================================
//  PersistenceManager.h  –  Persistance JSON sur disque
//  IFT585 – TP4
//  Fichiers gérés :
//    data/clients.json       – profils utilisateurs
//    data/directories.json   – répertoires partagés
//    data/invitations.json   – invitations en attente
//    data/files/{dir_id}/    – fichiers physiques
// =============================================================
#include <string>
#include <vector>
#include <mutex>
#include "../common/json.h"
#include "../common/FileMetadata.h"

struct ClientProfile {
    std::string username;
    std::string password_hash; // SHA-256
    std::string status;        // "online" | "offline"
    std::string session_token; // UUID v4 actif, "" si déconnecté
    long long   token_expiry;  // Unix timestamp (0 = pas d'expiration)
};

struct Directory {
    std::string              id;      // UUID v4
    std::string              name;
    std::string              admin;   // username de l'administrateur
    std::vector<std::string> members; // usernames des membres (admin inclus)
};

struct Invitation {
    std::string id;           // UUID v4
    std::string directory_id;
    std::string from_user;
    std::string to_user;
    std::string status;       // "pending" | "accepted" | "declined"
};

class PersistenceManager {
public:
    explicit PersistenceManager(const std::string& dataDir = "./data");

    // ---- Clients ----
    bool              clientExists(const std::string& username);
    ClientProfile     getClient(const std::string& username);
    void              saveClient(const ClientProfile& c);
    std::vector<ClientProfile> getAllClients();
    bool              validateToken(const std::string& username,
                                    const std::string& token);
    void              updateToken(const std::string& username,
                                  const std::string& token);
    void              updateStatus(const std::string& username,
                                   const std::string& status);

    // ---- Répertoires ----
    Directory              createDirectory(const std::string& name,
                                           const std::string& admin);
    Directory              getDirectory(const std::string& id);
    std::vector<Directory> getDirectoriesForUser(const std::string& username);
    void                   saveDirectory(const Directory& d);
    bool                   deleteDirectory(const std::string& id);
    bool                   isMember(const std::string& dirId,
                                    const std::string& username);
    bool                   isAdmin(const std::string& dirId,
                                   const std::string& username);
    void                   addMember(const std::string& dirId,
                                     const std::string& username);
    void                   removeMember(const std::string& dirId,
                                        const std::string& username);
    void                   transferAdmin(const std::string& dirId,
                                         const std::string& newAdmin);

    // ---- Invitations ----
    Invitation              createInvitation(const std::string& dirId,
                                             const std::string& from,
                                             const std::string& to);
    std::vector<Invitation> getPendingInvitations(const std::string& username);
    Invitation              getInvitation(const std::string& invId);
    bool                    acceptInvitation(const std::string& invId,
                                             const std::string& username);
    bool                    declineInvitation(const std::string& invId,
                                              const std::string& username);

    // ---- Métadonnées de fichiers ----
    std::vector<FileMetadata> getFilesMetadata(const std::string& dirId);
    void                      saveFileMetadata(const FileMetadata& meta);
    void                      deleteFileMetadata(const std::string& dirId,
                                                  const std::string& name);
    std::string               getFilePath(const std::string& dirId,
                                           const std::string& name);
    bool                      fileExists(const std::string& dirId,
                                          const std::string& name);

private:
    std::string dataDir_;
    std::mutex  mu_;

    // Helpers I/O JSON
    Json  readJson(const std::string& path);
    void  writeJson(const std::string& path, const Json& j);
    Json  readClients();
    Json  readDirectories();
    Json  readInvitations();
    void  writeClients(const Json& j);
    void  writeDirectories(const Json& j);
    void  writeInvitations(const Json& j);

    std::string makeUUID();
    void        ensureDir(const std::string& path);
};
