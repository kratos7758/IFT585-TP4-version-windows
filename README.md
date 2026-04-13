# IFT585 – TP4 : Système de partage de fichiers distribué
## Guide d'exécution et de correction

---

## Prérequis

- Windows 10 ou Windows 11 (64-bit)
- Git
- **Rien d'autre à installer** — tous les binaires et DLLs sont inclus dans le dépôt

---

## Étape 1 — Cloner le dépôt

```powershell
git clone https://github.com/kratos7758/IFT585-TP4-version-windows.git
cd IFT585-TP4-version-windows\ift585-tp
```

---

## Étape 2 — Lancer le serveur

Ouvrir un terminal PowerShell et le **laisser ouvert** pendant toute la durée des tests :

```powershell
cd server
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

## Étape 3 — Lancer les clients

Ouvrir **3 terminaux PowerShell séparés**. Dans chacun, exécuter :

```powershell
cd IFT585-TP4-version-windows\ift585-tp\bin
.\client_ift585.exe
```

> Si Windows affiche une alerte de sécurité SmartScreen, cliquer **"Informations complémentaires"** puis **"Exécuter quand même"**.

---

## Étape 4 — Scénarios de test

### TEST 1 — Authentification UDP (protocole stop-and-wait)

Dans chaque fenêtre client, entrer les informations suivantes et cliquer **Connexion** :

| Fenêtre | Serveur IP | Utilisateur | Mot de passe |
|---------|------------|-------------|--------------|
| Client 1 | 127.0.0.1 | alain | alain123 |
| Client 2 | 127.0.0.1 | marc | marc123 |
| Client 3 | 127.0.0.1 | moustapha | moust123 |

**Ce qu'on vérifie dans le terminal serveur :**
```
[AuthUDP] Nouvel utilisateur enregistré : alain
[AuthUDP] Authentification réussie pour : alain
[AuthUDP] Nouvel utilisateur enregistré : marc
[AuthUDP] Authentification réussie pour : marc
[AuthUDP] Nouvel utilisateur enregistré : moustapha
[AuthUDP] Authentification réussie pour : moustapha
```
Chaque `AUTH_REQ` reçoit un `AUTH_ACK` contenant un SessionToken UUID.

---

### TEST 2 — Création d'un répertoire partagé

Sur le client d'**alain** :
1. Cliquer **"+ Nouveau"**
2. Saisir : `Projet-IFT585`
3. Valider

**Ce qu'on vérifie :**
- Le répertoire `Projet-IFT585` apparaît dans la liste à gauche
- `alain` apparaît dans la liste des membres
- Le statut passe brièvement à "Synchronisation en cours..." puis "Synchronisé"
- Dans le terminal serveur : `POST /directories` reçu

---

### TEST 3 — Invitation d'un utilisateur

Sur le client d'**alain** :
1. Sélectionner `Projet-IFT585`
2. Cliquer **"Inviter"**
3. Saisir : `marc` → Valider

Sur le client de **marc** :
4. Une notification d'invitation apparaît → cliquer **Accepter**

**Ce qu'on vérifie :**
- `marc` apparaît dans la liste des membres de `Projet-IFT585`
- Dans le terminal serveur : `POST /invitations` puis `PUT /invitations/{id}/accept` reçus

---

### TEST 4 — Synchronisation de fichiers

Sur le client d'**alain** (répertoire `Projet-IFT585` sélectionné) :
1. Cliquer **"Envoyer un fichier"**
2. Choisir un fichier quelconque (ex. un fichier texte)

**Ce qu'on vérifie :**
- Le fichier apparaît dans la liste d'alain avec son hash SHA-256
- Dans le terminal serveur : `PUT /files/{dir_id}/{nom}` reçu
- Sur le client de **marc** : le fichier apparaît automatiquement (synchronisation polling)

---

### TEST 5 — Déconnexion

Sur le client d'**alain** :
1. Cliquer **"Déconnexion"**

**Ce qu'on vérifie dans le terminal serveur :**
```
[AuthUDP] Déconnexion de : alain
```

---

## Vérification des données persistées

Les fichiers JSON du serveur sont dans `server\data\` :

```powershell
# Utilisateurs enregistrés
type server\data\clients.json

# Répertoires partagés
type server\data\directories.json

# Invitations
type server\data\invitations.json
```

---

## Architecture du projet

| Composant | Protocole | Port | Rôle |
|-----------|-----------|------|------|
| Authentification | UDP stop-and-wait | 8888 | AUTH_REQ / AUTH_ACK / AUTH_NAK |
| API REST | TCP (HTTP) | 8080 | Répertoires, fichiers, invitations |
| Surveillance locale | - | - | Détection des changements de fichiers |
| Moteur de sync | - | - | Comparaison SHA-256, upload/download |

---

## En cas de problème

| Symptôme | Solution |
|----------|----------|
| La fenêtre client se ferme immédiatement | Vérifier que le dossier `bin\` contient bien les fichiers `.dll` |
| `Port already in use` | Changer les ports : `--tcp-port 8081 --udp-port 8889` |
| Alerte SmartScreen | Cliquer "Informations complémentaires" > "Exécuter quand même" |
| Le serveur ne démarre pas | Vérifier que les fichiers `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` sont dans `server\` |
