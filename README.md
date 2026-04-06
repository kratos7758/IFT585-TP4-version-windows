Guide de test complet – IFT585-TP4
Préparation : Lancer le serveur
Dans un terminal WSL dédié (ne jamais fermer) :


cd "/mnt/c/Users/Administrateur/Desktop/mes cours/IFT585 telematique/TP4/ift585-tp/server"
./server_ift585 --data ./data --tcp-port 8080 --udp-port 8888
Vous devez voir :


[INFO] Serveur UDP en écoute sur le port 8888
[INFO] Serveur REST en écoute sur le port 8080
[INFO] Pool de threads initialisé (4 workers)
[INFO] Serveur prêt. Appuyez sur Ctrl+C pour arrêter.
Laissez ce terminal ouvert toute la durée des tests.

TEST 1 — Authentification UDP (stop-and-wait)
Ouvrir 3 terminaux WSL supplémentaires (un par utilisateur)
Dans chaque nouveau terminal PowerShell → tapez wsl, puis :


cd "/mnt/c/Users/Administrateur/Desktop/mes cours/IFT585 telematique/TP4/ift585-tp/bin"
./client_ift585
Trois fenêtres Qt s'ouvrent.

Fenêtre de connexion – Utilisateur 1 : alain

Serveur IP  : 127.0.0.1
Utilisateur : alain
Mot de passe: alain123
→ Cliquer Connexion
Fenêtre de connexion – Utilisateur 2 : marc

Serveur IP  : 127.0.0.1
Utilisateur : marc
Mot de passe: marc123
→ Cliquer Connexion
Fenêtre de connexion – Utilisateur 3 : moustapha

Serveur IP  : 127.0.0.1
Utilisateur : moustapha
Mot de passe: moust123
→ Cliquer Connexion
Ce qu'on vérifie dans le terminal serveur :


[AuthUDP] Nouvel utilisateur enregistré : alain
[AuthUDP] Authentification réussie pour : alain
[AuthUDP] Nouvel utilisateur enregistré : marc
[AuthUDP] Authentification réussie pour : marc
[AuthUDP] Nouvel utilisateur enregistré : moustapha
[AuthUDP] Authentification réussie pour : moustapha
✅ Stop-and-wait validé : chaque AUTH_REQ reçoit un AUTH_ACK avec un SessionToken UUID.

TEST 2 — Création d'un répertoire partagé
Sur le client d'alain :
Cliquer "+ Nouveau"
Saisir : Projet-IFT585
Valider
Ce qu'on vérifie :

Le répertoire Projet-IFT585 apparaît dans la liste à gauche
Le membre alain apparaît dans la liste "Membres"
Le statut passe brièvement à "⟳ Synchronisation en cours..." puis revient à "● Synchronisé"
Dans le terminal serveur : POST /directories reçu
Sur le disque (terminal WSL d'alain) :


ls ~/IFT585-TP/
# → un dossier avec un UUID (ex: a1b2c3d4-...)
✅ Création de répertoire validée

TEST 3 — Invitation d'un utilisateur
Sur le client d'alain (admin du répertoire) :
Sélectionner Projet-IFT585 dans la liste
Cliquer "Inviter"
Saisir : marc
