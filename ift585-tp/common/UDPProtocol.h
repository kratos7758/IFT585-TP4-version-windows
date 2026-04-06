#pragma once
// =============================================================
//  UDPProtocol.h  –  Constantes et structures du protocole UDP
//  IFT585 – TP4   (stop-and-wait sur UDP brut)
// =============================================================
#include <string>
#include <cstdint>

// ---- Ports ----
constexpr int UDP_AUTH_PORT = 8888;
constexpr int TCP_REST_PORT = 80;

// ---- Types de messages UDP ----
constexpr const char* MSG_AUTH_REQ    = "AUTH_REQ";
constexpr const char* MSG_AUTH_ACK    = "AUTH_ACK";
constexpr const char* MSG_AUTH_NAK    = "AUTH_NAK";
constexpr const char* MSG_LOGOUT_REQ  = "LOGOUT_REQ";
constexpr const char* MSG_LOGOUT_ACK  = "LOGOUT_ACK";

// ---- Paramètres stop-and-wait ----
constexpr int UDP_TIMEOUT_SEC   = 2;   // Timeout de retransmission (secondes)
constexpr int UDP_MAX_RETRIES   = 3;   // Nombre max de tentatives
constexpr int UDP_MAX_PKT_SIZE  = 2048; // Taille max d'un datagramme

// ---- État machine à états serveur ----
enum class AuthState {
    WAITING,       // En attente d'un AUTH_REQ
    VALIDATING,    // Validation du hash en cours
    REPLIED        // Réponse envoyée (ACK ou NAK)
};

// ---- Structures de messages (sérialisées en JSON) ----
// AUTH_REQ  : { "type":"AUTH_REQ",  "seq":N, "username":"...", "password_hash":"..." }
// AUTH_ACK  : { "type":"AUTH_ACK",  "seq":N, "token":"...",    "message":"ok"       }
// AUTH_NAK  : { "type":"AUTH_NAK",  "seq":N, "message":"..."                        }
// LOGOUT_REQ: { "type":"LOGOUT_REQ","seq":N, "username":"...", "token":"..."         }
// LOGOUT_ACK: { "type":"LOGOUT_ACK","seq":N                                          }

struct AuthRequest {
    uint32_t    seq;
    std::string username;
    std::string password_hash; // SHA-256 du mot de passe
};

struct AuthAck {
    uint32_t    seq;
    std::string token;   // UUID v4 généré par le serveur
    bool        success;
    std::string message;
};
