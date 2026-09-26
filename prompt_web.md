# Prompt pour Antigravity : Interface WebUSB & Créateur de Pack NES (NumWorks)
Agis en tant qu'ingénieur front-end senior spécialisé dans les outils web pour matériel embarqué (WebUSB / WebDFU / manipulation binaire côté client).

## 1. Contexte du projet
Nous avons développé un sélecteur multi-ROMs bare-metal pour l'émulateur Nofrendo sur calculatrice NumWorks N0120 (STM32H725, écran 320×240). Les ROMs sont stockées en mémoire Flash OctoSPI dans la section .rodata et exécutées en XIP.

Dépôt cible : `LaPatateDeJP1/nofrendo-numworks`  
Hébergement visé : GitHub Pages (dossier `/docs`)

## 2. Objectif
Créer une Single Page Application (SPA) ultra-légère, autonome (HTML5, CSS3, JavaScript ES6 pur sans framework ni bundler), permettant :
- De communiquer avec la calculatrice via WebUSB pour installer l'émulateur en un clic.
- De permettre à l'utilisateur de composer son propre pack multi-ROMs dans son navigateur sans recompiler de code, en injectant la table des jeux et les données .nes directement dans le template binaire nofrendo.nwa avant l'envoi USB.

## 3. Direction Artistique & Ergonomie (Anti-AI Slop strict)
L'interface doit respecter une esthétique de minimalisme industriel inspirée de NumWorks et Teenage Engineering :
- **Palette chromatique** :
  - Fond de page : blanc cassé mat (`#F8F9FA`)
  - Cartes et blocs : blanc pur (`#FFFFFF`)
  - Bordures : 1 px solide net (`#E5E7EB`)
  - Typographie principale : noir profond (`#111827`)
  - Typographie secondaire / libellés : gris neutre (`#6B7280`)
  - Couleur d'accentuation unique : jaune chaud NumWorks (`#FFC300` ou `#E5A500`), réservée aux boutons d'action clés et aux jauges.
- **Interdictions graphiques** :
  - Aucun dégradé violet/cyan néon.
  - Aucun effet de flou ou de transparence (glassmorphism).
  - Aucune ombre diffuse lourde.
  - Aucune particule ni animation superflue.
  - Aucun texte promotionnel creux : phrases courtes, techniques et directes.
- **Typographie** : pile sans-serif système moderne (`system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif`).
- **Navigation** : barre d'onglets textuelle sobre en haut de page pour basculer entre les trois vues sans rechargement.

## 4. Spécifications fonctionnelles (3 Vues)
### Onglet 1 : « Flash Express »
- Destiné aux utilisateurs voulant une installation immédiate sans fournir de fichiers.
- Embarque un pack par défaut de homebrews libres (ex. `2048.nes`).
- Bouton principal : « Installer le pack sur la NumWorks ».
- Console d'état compacte : statut de la connexion WebUSB, progression du transfert et confirmation.

### Onglet 2 : « Créateur de Pack » (Constructeur dynamique client-side)
- Zone de glisser-déposer (Drag & Drop) : Accepte les fichiers `.nes` et vérifie l'en-tête iNES standard (`NES\x1A`).
- Gestionnaire de liste :
  - Titre extrait du nom de fichier, modifiable en un clic.
  - Poids individuel affiché en Ko.
  - Boutons pour réordonner (monter/descendre) et bouton pour retirer un jeu.
- Jauge d'espace mémoire : Barre linéaire indiquant l'espace Flash externe utilisé sur une limite de 2 Mo (ex. 1,1 Mo / 2,0 Mo alloués).
- Logique d'assemblage binaire (JS pur côté client) :
  - Concaténation des ROMs et injection de la structure d'indexation (`GameEntry`) dans le template binaire `nofrendo.nwa`.
- Bouton d'action : « Générer et Flasher ».

### Onglet 3 : « Commandes & Sécurité »
- Tableau récapitulatif compact du mapping des touches (Croix directionnelle, boutons A/B, Start, Select).
- Mise en avant de la touche d'urgence (Retour ou Home) qui coupe le jeu et retourne au menu en une fraction de seconde.
- Note pédagogique rassurante : l'application n'altère pas l'OS officiel et est neutralisée en mode examen pour le baccalauréat.

## 5. Cadre Légal & Mentions (Modèle BYOR irréprochable)
- Modèle BYOR (Bring Your Own ROM) strict : aucun jeu commercial sous copyright n'est hébergé ou distribué.
- Aucun nom de licence déposée ni aucun lien vers des sites de téléchargement contrefaits.
- Liens de référence limités aux catalogues légaux indépendants (ex. NESdev, itch.io section NES).
- Mention de bas de page obligatoire :
  > « Mentions légales : Nofrendo est un émulateur distribué sous licence GPL. Ce projet open source indépendant n'est ni affilié à, ni approuvé par NumWorks ou Nintendo. Cette plateforme ne distribue ni n'héberge aucun contenu protégé par le droit d'auteur. L'utilisateur est seul responsable des fichiers de sauvegarde personnelle (.nes) qu'il choisit d'intégrer dans son appareil. »

## 6. Couche WebUSB
- Utilisation de l'API standard `navigator.usb`.
- Détection du Vendor ID STMicroelectronics / NumWorks (`0x0483`).
- Gestion propre des différents états : périphérique non détecté, calculatrice verrouillée, transfert par blocs avec pourcentage, et alerte si le navigateur n'est pas basé sur Chromium (Chrome, Brave, Edge requis).

## 7. Structure des fichiers & Déploiement
Fournis les sources complètes, propres et commentées, à placer dans le dossier `docs/` pour un déploiement direct via GitHub Pages :
- `docs/index.html` : Structure sémantique propre.
- `docs/style.css` : Feuille de style complète respectant le design system minimaliste.
- `docs/app.js` : Logique de l'interface, gestion des onglets, drag & drop et injection binaire.
- `docs/webusb.js` : Module de communication USB avec la calculatrice.
Indique les commandes Git pour activer GitHub Pages sur la branche principale (`main` / dossier `/docs`).
