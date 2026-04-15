# IFT585 – TP4 : Système de partage de fichiers distribué
## Windows 10/11 – Guide d'exécution et de compilation

---

## Comptes de test disponibles

| Utilisateur | Mot de passe |
|-------------|-------------|
| alain      | alain    |
| marc        | marc     |
| moustapha       | moustapha   |
| charlie     | charlie123  |

> Les mots de passe sont stockés sous forme de hash SHA-256 dans `ift585-tp/server/data/clients.json`.
> Pour ajouter un utilisateur, il suffit d'y ajouter une entrée avec le hash SHA-256 de son mot de passe.

---

## OPTION A — Exécuter sans compiler (exécutables fournis)

Les exécutables Windows compilés sont déjà inclus dans le dépôt.
**Aucune installation requise** pour cette option.

### A.1 Lancer le serveur

Ouvrir un terminal **en tant qu'administrateur** (requis pour le port 80) :

```
cd ift585-tp\server
server_ift585.exe
```

Sortie attendue :
```
=== IFT585 TP4 – Serveur de partage de fichiers distribué ===
[CONFIG] Données : ./data
[CONFIG] Port UDP : 8888
[CONFIG] Port TCP : 80
[INFO] Serveur UDP en écoute sur le port 8888
[INFO] Serveur REST en écoute sur le port 80
[INFO] Pool de threads initialisé (4 workers)
[INFO] Serveur prêt. Appuyez sur Ctrl+C pour arrêter.
```

> Laisser cette fenêtre ouverte pendant toute la durée des tests.

### A.2 Lancer les clients

Ouvrir **autant de terminaux que de clients** souhaités. Dans chacun :

```
cd ift585-tp\bin
client_ift585.exe
```

> Si Windows affiche une alerte SmartScreen : cliquer **"Informations complémentaires"** → **"Exécuter quand même"**.

### A.3 Se connecter

Dans la fenêtre de connexion qui s'ouvre :

| Champ        | Valeur        |
|--------------|---------------|
| Serveur IP   | 127.0.0.1 (local) ou IP de la machine serveur |
| Utilisateur  | alice / bob / charlie |
| Mot de passe | alice123 / bob123 / charlie123 |

---

## OPTION B — Compiler puis exécuter

### B.1 Logiciels à installer

#### 1. CMake (version 3.16 ou plus récente)

Télécharger l'installeur Windows sur [cmake.org/download](https://cmake.org/download/).
Cocher **"Add CMake to PATH"** lors de l'installation.

Vérification :
```
cmake --version
```

#### 2. MinGW-W64 — GCC 64-bit (compilateur C++)

Télécharger la version **x86_64 – posix – seh** depuis [winlibs.com](https://winlibs.com/).
Extraire l'archive dans `C:\MinGW\mingw64\`.

Le compilateur doit se trouver à :
```
C:\MinGW\mingw64\bin\g++.exe
```

Vérification :
```
C:\MinGW\mingw64\bin\g++.exe --version
```

> **Important** : ne pas utiliser le MinGW 32-bit de mingw.org (`C:\MinGW\bin\g++.exe`).
> Il ne supporte pas `std::thread` et `std::mutex`. Il faut obligatoirement la version 64-bit.

#### 3. Qt 6.4.2 – MinGW 64-bit

Télécharger l'installeur Qt depuis [qt.io](https://www.qt.io/download-qt-installer) (compte gratuit requis).
Dans l'installeur, sélectionner : **Qt 6.4.2 → MinGW 64-bit**.

Qt doit s'installer dans :
```
C:\Qt\6.4.2\mingw_64\
```

Vérification :
```
C:\Qt\6.4.2\mingw_64\bin\qmake.exe --version
```

---

### B.2 Compiler le serveur

Ouvrir un terminal et exécuter depuis la racine du projet :

```
cmake -S ift585-tp/server -B build-server -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=C:/MinGW/mingw64/bin/g++.exe -DCMAKE_C_COMPILER=C:/MinGW/mingw64/bin/gcc.exe -DCMAKE_BUILD_TYPE=Release

cmake --build build-server --config Release
```

Résultat attendu :
```
[100%] Built target server_ift585
```

Binaire produit : `ift585-tp\server\server_ift585.exe`

---

### B.3 Compiler le client

```
cmake -S ift585-tp/client -B build-client -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=C:/MinGW/mingw64/bin/g++.exe -DCMAKE_C_COMPILER=C:/MinGW/mingw64/bin/gcc.exe -DCMAKE_PREFIX_PATH=C:/Qt/6.4.2/mingw_64 -DCMAKE_BUILD_TYPE=Release

cmake --build build-client --config Release
```

Résultat attendu :
```
[100%] Built target client_ift585
```

Binaire produit : `ift585-tp\bin\client_ift585.exe`

---

### B.4 Déployer les DLLs Qt (une seule fois après compilation)

```
C:\Qt\6.4.2\mingw_64\bin\windeployqt.exe ift585-tp\bin\client_ift585.exe
```

Cette commande copie automatiquement toutes les DLLs Qt nécessaires dans `ift585-tp\bin\`.

Copier également les DLLs MinGW pour le serveur :
```
copy C:\MinGW\mingw64\bin\libgcc_s_seh-1.dll  ift585-tp\server\
copy C:\MinGW\mingw64\bin\libstdc++-6.dll      ift585-tp\server\
copy C:\MinGW\mingw64\bin\libwinpthread-1.dll  ift585-tp\server\
```

---

### B.5 Lancer le serveur et les clients

Suivre les étapes **A.1** et **A.2** ci-dessus.

---

## Scénarios de test

### TEST 1 — Authentification UDP (protocole stop-and-wait)

Lancer 3 clients, se connecter avec alain, marc, charlie sur `127.0.0.1`.

Dans le terminal serveur, vérifier :
```
[UDP] Authentification réussie pour : alain
[UDP] Authentification réussie pour : marc
[UDP] Authentification réussie pour : charlie
```

---

### TEST 2 — Création d'un répertoire partagé

Sur le client **alain** :
1. Cliquer **"+ Nouveau"**
2. Saisir un nom (ex: `Projet-TP4`)
3. Valider

Résultat : le répertoire apparaît dans la liste, alice en est l'administrateur.

---

### TEST 3 — Invitation d'un utilisateur

Sur le client **alain** :
1. Sélectionner `Projet-TP4`
2. Cliquer **"Inviter"** → choisir `marc` dans la liste des utilisateurs en ligne
3. Valider

Sur le client **alain** :
1. Onglet **Notifications** → invitation visible : `De alice → "Projet-TP4"`
2. Cliquer **"Accepter"**

Résultat : `marc` apparaît dans la liste des membres du répertoire.

---

### TEST 4 — Transfert et synchronisation de fichiers

Sur la machine d'**alain**, copier un fichier dans le dossier local synchronisé :
```
C:\Users\{VotreNom}\IFT585-TP\{uuid-du-répertoire}\
```
L'UUID est visible dans le journal ou dans `server/data/directories.json`.

Résultat :
- Onglet **Journal** (alain) : `Upload OK : monfichier.txt (1234 octets)`
- Onglet **Fichiers** (alain) : le fichier apparaît avec sa taille et son hash SHA-256
- Sur le client **marc** : le fichier se synchronise automatiquement (max 30 secondes)
- Onglet **Journal** (marc) : `Download OK : monfichier.txt (1234 octets)`

---

### TEST 5 — Déconnexion propre

Sur le client **alain** :
1. Cliquer **"Déconnexion"**

Dans le terminal serveur :
```
[UDP] Déconnexion de : alice
```

`data/clients.json` : le statut d'alice passe à `"offline"`.

---

## Ports utilisés

| Protocole | Port | Usage |
|-----------|------|-------|
| UDP       | 8888 | Authentification (stop-and-wait) |
| TCP       | 80   | API REST (transfert de fichiers, répertoires, invitations) |

> Le port TCP 80 nécessite des droits **administrateur** sur Windows.
> Pour changer les ports : `server_ift585.exe --udp-port 8888 --tcp-port 80`

---

## Structure du projet

```
ift585-tp/
├── common/                  Fichiers partagés client/serveur
│   ├── platform.h           Abstraction Windows/Linux (sockets, threads)
│   ├── sha256.h / .cpp      Calcul de hash SHA-256
│   ├── json.h               Sérialiseur JSON minimal
│   ├── FileMetadata.h       Structure de métadonnées de fichier
│   └── UDPProtocol.h        Constantes du protocole UDP
│
├── client/                  Code source du client
│   ├── main.cpp             Point d'entrée
│   ├── ClientApp.h/cpp      Coordinateur principal
│   ├── NetworkProvider.h/cpp   UDP stop-and-wait + client REST HTTP/1.1
│   ├── SurveillanceLocale.h/cpp  Surveillance du dossier (ReadDirectoryChangesW)
│   ├── SyncEngine.h/cpp     Moteur de synchronisation (polling 30 s + événements)
│   ├── InterfaceUtilisateur.h/cpp  Interface Qt (login, tableau de bord)
│   └── CMakeLists.txt
│
├── server/                  Code source du serveur
│   ├── main.cpp             Point d'entrée
│   ├── ServerCore.h/cpp     Cœur du serveur
│   ├── AuthUDPHandler.h/cpp Authentification UDP stop-and-wait
│   ├── RESTServer.h/cpp     Serveur HTTP/1.1 manuel (sans framework)
│   ├── PersistenceManager.h/cpp  Persistance JSON sur disque
│   ├── server_ift585.exe    Exécutable Windows (prêt à l'emploi)
│   ├── libgcc_s_seh-1.dll   DLL MinGW requise
│   ├── libstdc++-6.dll      DLL MinGW requise
│   ├── libwinpthread-1.dll  DLL MinGW requise
│   ├── CMakeLists.txt
│   └── data/
│       ├── clients.json     Comptes utilisateurs (alain, marc, charlie)
│       ├── directories.json Répertoires partagés
│       ├── invitations.json Invitations en attente
│       └── files/           Fichiers synchronisés (créé automatiquement)
│
└── bin/                     Client Windows déployé (prêt à l'emploi)
    ├── client_ift585.exe
    ├── Qt6Core.dll
    ├── Qt6Gui.dll
    ├── Qt6Widgets.dll
    └── ...                  (autres DLLs Qt)
```
