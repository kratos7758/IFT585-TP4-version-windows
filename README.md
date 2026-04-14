# Guide de compilation et de test – IFT585-TP4
## Windows natif (sans WSL)

---

## 1. Prérequis – Logiciels à installer

### 1.1 CMake
Télécharger et installer CMake depuis cmake.org (version 3.16 ou plus récente).
Cocher "Add CMake to PATH" lors de l'installation.

Vérification :
```
cmake --version
```

### 1.2 MinGW-W64 (compilateur C++)
Un MinGW-W64 récent doit être installé. Sur cette machine il se trouve dans :
```
C:\MinGW\mingw64\bin\g++.exe   (GCC 15.1.0)
```
Si absent, télécharger la version "x86_64 - posix - seh" depuis winlibs.com et extraire dans C:\MinGW\mingw64.

Vérification :
```
C:\MinGW\mingw64\bin\g++.exe --version
```

### 1.3 Qt 6.4.2
Qt est installé dans C:\Qt\6.4.2\mingw_64 sur cette machine.
Si absent, installer via Chocolatey (PowerShell administrateur) :
```
choco install qt6-base-dev -y
```
Ou télécharger l'installeur depuis qt.io (compte gratuit requis) et sélectionner Qt 6.x > MinGW 64-bit.

Vérification :
```
C:\Qt\6.4.2\mingw_64\bin\qmake.exe --version
```

---

## 2. Compilation

Ouvrir **PowerShell** et exécuter les commandes suivantes.

### 2.1 Compiler le serveur

```powershell
cd "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp"

Remove-Item -Recurse -Force build-server -ErrorAction SilentlyContinue
mkdir build-server
cd build-server

cmake ..\server -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:/MinGW/mingw64/bin/g++.exe"

mingw32-make
```

Résultat attendu : `[100%] Built target server_ift585`
Binaire produit : `ift585-tp\server\server_ift585.exe`

### 2.2 Compiler le client

```powershell
cd "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp"

Remove-Item -Recurse -Force build-client -ErrorAction SilentlyContinue
mkdir build-client
cd build-client

cmake ..\client -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:/MinGW/mingw64/bin/g++.exe" -DCMAKE_PREFIX_PATH="C:/Qt/6.4.2/mingw_64"

mingw32-make
```

Résultat attendu : `[100%] Built target client_ift585`
Binaire produit : `ift585-tp\bin\client_ift585.exe`

### 2.3 Déployer les DLLs Qt (une seule fois)

```powershell
cd "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\bin"

C:\Qt\6.4.2\mingw_64\bin\windeployqt.exe .\client_ift585.exe
```

Cette commande copie automatiquement toutes les DLLs Qt nécessaires à côté de l'exécutable.

---

## 3. Lancer le serveur

Ouvrir un **nouveau terminal PowerShell** et le laisser ouvert pendant tous les tests :

```powershell
cd "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\server"

.\server_ift585.exe --data .\data --tcp-port 8080 --udp-port 8888
```

Sortie attendue :
```
=== IFT585 TP4 - Serveur de partage de fichiers distribué ===
[CONFIG] Données : ./data
[CONFIG] Port UDP : 8888
[CONFIG] Port TCP : 8080
[INFO] Serveur UDP en écoute sur le port 8888
[INFO] Serveur REST en écoute sur le port 8080
[INFO] Pool de threads initialisé (4 workers)
[INFO] Serveur prêt. Appuyez sur Ctrl+C pour arrêter.
```

---

## 4. Lancer les clients

Ouvrir **3 terminaux PowerShell séparés**. Dans chacun :

```powershell
cd "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\bin"

.\client_ift585.exe
```

> Si Windows affiche une alerte SmartScreen, cliquer "Informations complémentaires" puis "Exécuter quand même".

---

## 5. Scénarios de test

### TEST 1 — Authentification UDP (stop-and-wait)

Dans chaque fenêtre cliente, entrer :

| Champ       | Utilisateur 1 | Utilisateur 2 | Utilisateur 3 |
|-------------|---------------|---------------|---------------|
| Serveur IP  | 127.0.0.1     | 127.0.0.1     | 127.0.0.1     |
| Utilisateur | alain         | marc          | moustapha     |
| Mot de passe| alain123      | marc123       | moust123      |

Cliquer **Connexion** dans chaque fenêtre.

Ce qu'on vérifie dans le terminal serveur :
```
[AuthUDP] Nouvel utilisateur enregistré : alain
[AuthUDP] Authentification réussie pour : alain
[AuthUDP] Nouvel utilisateur enregistré : marc
[AuthUDP] Authentification réussie pour : marc
[AuthUDP] Nouvel utilisateur enregistré : moustapha
[AuthUDP] Authentification réussie pour : moustapha
```
Chaque AUTH_REQ reçoit un AUTH_ACK avec un SessionToken UUID.

---

### TEST 2 — Création d'un répertoire partagé

Sur le client d'**alain** :
1. Cliquer **"+ Nouveau"**
2. Saisir : `Projet-IFT585`
3. Valider

Ce qu'on vérifie :
- Le répertoire `Projet-IFT585` apparaît dans la liste à gauche
- Le membre `alain` apparaît dans la liste "Membres"
- Le statut passe brièvement à "Synchronisation en cours..." puis revient à "Synchronisé"
- Dans le terminal serveur : `POST /directories` reçu

---

### TEST 3 — Invitation d'un utilisateur

Sur le client d'**alain** :
1. Sélectionner `Projet-IFT585` dans la liste
2. Cliquer **"Inviter"**
3. Saisir : `marc`
4. Valider

Ce qu'on vérifie :
- Sur le client de **marc** : une notification d'invitation apparaît
- marc clique **Accepter**
- `marc` apparaît dans la liste des membres de `Projet-IFT585`
- Dans le terminal serveur : `POST /invitations` puis `PUT /invitations/{id}/accept` reçus

---

### TEST 4 — Synchronisation de fichiers

Sur le client d'**alain** (répertoire `Projet-IFT585` sélectionné) :
1. Cliquer **"Envoyer un fichier"** (ou glisser-déposer un fichier)
2. Choisir n'importe quel fichier texte

Ce qu'on vérifie :
- Le fichier apparaît dans la liste des fichiers d'alain
- Dans le terminal serveur : `PUT /files/{dir_id}/{nom}` reçu
- Sur le client de **marc** : le fichier apparaît automatiquement (sync polling 30 s ou push)

---

### TEST 5 — Déconnexion

Sur le client d'**alain** :
1. Cliquer **"Déconnexion"**

Ce qu'on vérifie dans le terminal serveur :
```
[AuthUDP] Déconnexion de : alain
```
Le statut d'alain passe à "offline" dans les données serveur (`data/clients.json`).

---

## 6. Vérification des données serveur

Les données persistées sont dans `ift585-tp\server\data\` :

```powershell
# Voir les utilisateurs enregistrés
type "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\server\data\clients.json"

# Voir les répertoires créés
type "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\server\data\directories.json"

# Voir les invitations
type "C:\Users\Administrateur\Desktop\mes cours\IFT585 telematique\TP4\iftversionmous\ift585-tp\server\data\invitations.json"
```

---

## 7. Résolution de problèmes

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| `client_ift585.exe` se ferme immédiatement | DLLs Qt manquantes | Relancer `windeployqt.exe` (section 2.3) |
| `std::mutex` does not name a type | Vieux MinGW (mingw.org) | Utiliser `C:/MinGW/mingw64/bin/g++.exe` |
| `ssize_t` conflicting declaration | MinGW-W64 64-bit redéfinit `ssize_t` | Déjà corrigé dans `platform.h` |
| Port déjà utilisé | Un autre processus utilise 8080 ou 8888 | Changer les ports avec `--tcp-port` et `--udp-port` |
| Alerte SmartScreen au lancement | Exécutable non signé | Cliquer "Informations complémentaires" > "Exécuter quand même" |
