#!/bin/bash
# =============================================================
#  build.sh  –  Compilation complète IFT585-TP4
#  Usage depuis WSL : bash build.sh [server|client|all]
# =============================================================
set -e

TARGET=${1:-all}

build_server() {
    echo "=== Compilation du serveur ==="
    cd server/ && make
    cd ..
    echo "==> Serveur compilé : server/server_ift585"
}

build_client() {
    echo "=== Compilation du client ==="
    cd client/ && qmake client.pro && make
    cd ..
    echo "==> Client compilé : client/client_ift585"
}

case "$TARGET" in
    server) build_server ;;
    client) build_client ;;
    *)
        build_server
        build_client
        ;;
esac

echo ""
echo "=== Compilation terminée ==="
echo "Lancer le serveur : cd server/ && ./server_ift585 --data ./data --tcp-port 8080 --udp-port 8888"
echo "Lancer le client  : cd client/ && ./client_ift585"
