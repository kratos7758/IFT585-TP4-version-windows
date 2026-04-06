#pragma once
// =============================================================
//  FileMetadata.h  –  Structure partagée client/serveur
//  IFT585 – TP4
// =============================================================
#include <string>
#include <ctime>
#include "../common/json.h"

struct FileMetadata {
    std::string name;       // Nom du fichier (sans chemin)
    std::string dir_id;     // Identifiant du répertoire partagé
    std::string hash;       // SHA-256 du contenu
    long long   size   = 0; // Taille en octets
    long long   mtime  = 0; // Horodatage de dernière modification (Unix timestamp)
    bool        deleted = false; // Marqueur de suppression logique

    // ---- Sérialisation vers Json ----
    Json toJson() const {
        Json j = Json::object();
        j["name"]    = Json(name);
        j["dir_id"]  = Json(dir_id);
        j["hash"]    = Json(hash);
        j["size"]    = Json(size);
        j["mtime"]   = Json(mtime);
        j["deleted"] = Json(deleted);
        return j;
    }

    // ---- Désérialisation depuis Json ----
    static FileMetadata fromJson(const Json& j) {
        FileMetadata m;
        if (j.contains("name"))    m.name    = j.at("name").get_string();
        if (j.contains("dir_id"))  m.dir_id  = j.at("dir_id").get_string();
        if (j.contains("hash"))    m.hash    = j.at("hash").get_string();
        if (j.contains("size"))    m.size    = j.at("size").get_ll();
        if (j.contains("mtime"))   m.mtime   = j.at("mtime").get_ll();
        if (j.contains("deleted")) m.deleted = j.at("deleted").get_bool();
        return m;
    }
};
