# IFT585 – TP4 : Guide de correction
## Aucune installation requise — Windows 10/11 64-bit uniquement

---

## Étape 1 — Cloner le dépôt

```powershell
git clone https://github.com/kratos7758/IFT585-TP4-version-windows.git
cd IFT585-TP4-version-windows\ift585-tp
```

---

## Étape 2 — Lancer le serveur

Ouvrir un terminal PowerShell et le **laisser ouvert** pendant toute la correction :

```powershell
cd server
.\server_ift585.exe --data .\data --tcp-port 8080 --udp-port 8888
```

Sortie attendue :
```
=== IFT585 TP4 - Serveur de partage de fichiers distribué ===
[INFO] Serveur UDP en écoute sur le port 8888
[INFO] Serveur REST en écoute sur le port 8080
[INFO] Pool de threads initialisé (4 workers)
[INFO] Serveur prêt. Appuyez sur Ctrl+C pour arrêter.
```

---

## Étape 3 — Lancer 3 clients

Ouvrir **3 terminaux PowerShell séparés**. Dans chacun :

```powershell
cd IFT585-TP4-version-windows\ift585-tp\bin
.\client_ift585.exe
```

> Si Windows affiche une alerte SmartScreen : **"Informations complémentaires" → "Exécuter quand même"**

---

## Étape 4 — Tests

### TEST 1 — Authentification UDP (stop-and-wait)

Se connecter dans chaque fenêtre :

| Fenêtre  | Serveur IP | Utilisateur | Mot de passe |
|----------|------------|-------------|--------------|
| Client 1 | 127.0.0.1  | alain       | alain123     |
| Client 2 | 127.0.0.1  | marc        | marc123      |
| Client 3 | 127.0.0.1  | moustapha   | moust123     |

Cliquer **Connexion** dans chaque fenêtre.

Vérifier dans le terminal serveur :
```
[AuthUDP] Nouvel utilisateur enregistré : alain
[AuthUDP] Authentification réussie pour : alain
[AuthUDP] Nouvel utilisateur enregistré : marc
[AuthUDP] Authentification réussie pour : marc
[AuthUDP] Nouvel utilisateur enregistré : moustapha
[AuthUDP] Authentification réussie pour : moustapha
```

---

### TEST 2 — Création d'un répertoire partagé

Sur le client d'**alain** :
1. Cliquer **"+ Nouveau"** → saisir `Projet-IFT585` → Valider

Vérifier :
- `Projet-IFT585` apparaît dans la liste
- `alain` est listé comme membre
- Terminal serveur : `POST /directories` reçu

---

### TEST 3 — Invitation

Sur le client d'**alain** :
1. Sélectionner `Projet-IFT585` → cliquer **"Inviter"** → saisir `marc` → Valider

Sur le client de **marc** :
2. Accepter l'invitation

Vérifier :
- `marc` apparaît dans les membres de `Projet-IFT585`
- Terminal serveur : `POST /invitations` puis `PUT /invitations/{id}/accept`

---

### TEST 4 — Synchronisation de fichiers

Sur le client d'**alain** (répertoire `Projet-IFT585` sélectionné) :
1. Cliquer **"Envoyer un fichier"** → choisir un fichier texte

Vérifier :
- Le fichier apparaît dans la liste avec son hash SHA-256
- Terminal serveur : `PUT /files/{dir_id}/{nom}` reçu
- Sur le client de **marc** : le fichier apparaît automatiquement

---

### TEST 5 — Déconnexion

Sur le client d'**alain** :
1. Cliquer **"Déconnexion"**

Vérifier dans le terminal serveur :
```
[AuthUDP] Déconnexion de : alain
```

---

## Vérification des données persistées

```powershell
type server\data\clients.json
type server\data\directories.json
type server\data\invitations.json
```

---

## Problèmes connus

| Symptôme | Solution |
|----------|----------|
| Fenêtre client se ferme sans s'ouvrir | Vérifier que `bin\` contient les fichiers `.dll` |
| Port déjà utilisé | Changer avec `--tcp-port 8081 --udp-port 8889` |
| Alerte SmartScreen | "Informations complémentaires" → "Exécuter quand même" |
