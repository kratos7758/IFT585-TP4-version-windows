#pragma once
// =============================================================
//  sha256.h  –  Interface SHA-256 portable (sans dépendance)
//  IFT585 – TP4
// =============================================================
#include <string>
#include <cstdint>

namespace SHA256 {
    // Calcule le hash SHA-256 d'une chaîne brute et retourne l'hexadécimal (64 chars)
    std::string hash(const std::string& data);

    // Calcule le hash SHA-256 d'un tampon binaire
    std::string hash(const unsigned char* data, size_t len);

    // Calcule le hash SHA-256 d'un fichier sur le disque
    std::string hashFile(const std::string& path);
}
