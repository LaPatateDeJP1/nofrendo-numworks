# Nofrendo - Émulateur NES pour NumWorks N0120

Portage optimisé et enrichi de l'émulateur Nintendo Entertainment System (**Nofrendo 1.2.3**) pour calculatrice graphique **NumWorks N0120** (architecture STM32H725VET6, Cortex-M7 à 550 MHz, écran LCD 320×240, Flash externe OctoSPI).

Cette version apporte un **sélecteur interactif multi-ROMs**, un **menu Pause OSD en jeu**, l'accélération **Fast-Forward 2x**, la mise à l'échelle **plein écran (320×240)**, des **palettes de couleurs rétro** commutables à la volée, et une gestion mémoire **XIP** (stockage direct en Flash externe sans consommation de RAM).

---

## 🌟 Fonctionnalités

- **Catalogue Multi-ROMs Interactif** :
  - Interface plein écran 320×240 avec pagination, défilement fluide et boucle circulaire.
  - Troncature dynamique propre des titres pour éviter tout débordement.
  - Inspection automatique des métadonnées cartouche (numéro et nom du **Mapper iNES**, tailles **PRG** / **CHR**, poids en Ko).
  - Boucle d'exécution continue : quitter un jeu ramène directement au menu de sélection.

- **Mémoire Flash XIP (Zero RAM Waste)** :
  - Les ROMs sont compilées directement dans la section `.rodata` de la Flash externe OctoSPI avec alignement 32 bits.
  - Empreinte RAM interne (.data + .bss) fixée à seulement **~16.8 Ko** sur 1 Mo, quel que soit le nombre de jeux embarqués.

- **Menu Pause OSD en jeu** :
  - Déclenché à tout moment avec **`Toolbox`**, **`Var`** ou le raccourci **`Shift` + `Retour`**.
  - Pop-up centré avec reprise instantanée, modification de vitesse, de format et de palette, réinitialisation à chaud (**Reset**) et retour au catalogue.

- **Moteur d'Affichage & Mise à l'Échelle** :
  - **Mode 4:3 Original (256×240)** : Centré à l'écran avec bordures latérales rétro stylisées aux couleurs de la NES.
  - **Mode Plein Écran (320×240)** : Étirement horizontal 5:4 fluide tirant parti des 550 MHz du Cortex-M7 sans baisse de framerate.

- **5 Palettes de Couleurs Rétro** :
  - **Originale NES** : Rendu historique Nofrendo.
  - **Smooth Composite (FirebrandX)** : Couleurs douces et naturelles façon tube cathodique.
  - **Arcade Vivid (Sony CXA)** : Rendu éclatant et contrasté.
  - **Game Boy Rétro (DMG-01)** : Monochrome vert olive iconique à 4 nuances.
  - **Noir & Blanc** : Écran de télévision cathodique rétro.

- **Avance Rapide (Fast-Forward 2x)** :
  - Doublement dynamique de la cadence d'émulation (120 FPS) pour accélérer cinématiques et dialogues.

- **Compatibilité Epsilon 24+** :
  - Code bare-metal blindé respectant l'isolation de la MPU (*Memory Protection Unit*) d'Epsilon sans aucun HardFault.

---

## 🎮 Commandes & Contrôles

### En jeu (Émulateur NES)
| Touche NumWorks | Bouton NES | Rôle |
| :--- | :---: | :--- |
| **Flèches directionnelles** | **D-Pad** | Déplacements (Haut, Bas, Gauche, Droite) |
| **Back** (`<--`) | **Bouton A** | Action principale / Saut |
| **OK** | **Bouton B** | Action secondaire / Attaque / Course |
| **EXE** | **Bouton A** | Action principale (pavé numérique alternatif) |
| **Ans** | **Bouton B** | Action secondaire (pavé numérique alternatif) |
| **Shift** | **Select** | Sélection / Menu interne du jeu |
| **Backspace** (`[X]`) | **Start** | Démarrer la partie / Pause interne NES |
| **Toolbox** / **Var** | **Menu OSD** | Ouvre le menu Pause Nofrendo |
| **Shift** + **Back** | **Menu OSD** | Raccourci alternatif pour ouvrir la Pause |
| **tan** | **Hard Reset** | Réinitialisation matérielle du jeu |

### Dans le Sélecteur Multi-ROMs
| Touche NumWorks | Action |
| :--- | :--- |
| **Haut** / **Bas** | Déplacer le curseur ligne par ligne |
| **Gauche** / **Droite** | Saut de page précédente / suivante |
| **OK** ou **EXE** | Lancer le jeu sélectionné |
| **Home** | Quitter vers le système Epsilon de la calculatrice |

---

## 📥 Installation

### Option 1 : Via l'Atelier NumWorks (Recommandé)
1. Ouvrez Google Chrome ou Edge et rendez-vous sur **[my.numworks.com/apps](https://my.numworks.com/apps)**.
2. Connectez votre calculatrice NumWorks avec son câble USB.
3. Glissez-déposez le fichier binaire **`output/nofrendo.nwa`** sur la page.
4. Cliquez sur **Installer sur la calculatrice**.

### Option 2 : En ligne de commande (nwlink)
Dans un terminal avec Node.js installé :
```powershell
npx --yes -- nwlink@0.0.19 install-nwa output/nofrendo.nwa
```

---

## 🛠️ Compilation

### Prérequis
- **Toolchain ARM Embedded** : `arm-none-eabi-gcc` accessible dans votre variable `PATH`.
- **Node.js** : pour l'outil de packaging `nwlink`.
- **Python 3.8+** : pour l'automatisation du catalogue et du build.

### Compiler l'application
Exécutez simplement le script de compilation unifié :
```bash
python build.py
```
Le script compile tous les modules C, génère l'icône, lie le binaire `.nwa` et produit l'exécutable pour la NumWorks dans `output/nofrendo.nwa`.

---

## 🕹️ Personnaliser le Catalogue de Jeux

Pour ajouter vos propres jeux NES :

1. Déposez vos fichiers de ROMs au format standard `.nes` dans le dossier **`roms/`**.
2. Régénérez le catalogue C :
   ```bash
   python tools/embed_roms.py -i roms -c src/rom_catalog.c
   ```
3. Recompilez le projet :
   ```bash
   python build.py
   ```
4. Flashez le nouveau binaire généré dans `output/nofrendo.nwa` sur votre calculatrice.

---

## 📐 Architecture Technique

- **Cible matérielle** : NumWorks modèle N0120 (SoC STM32H725VET6, Cortex-M7 @ 550 MHz).
- **Affichage** : Dalle LCD 320×240 en RGB565 via l'API bare-metal EADK.
- **Stockage ROM** : Flash externe OctoSPI XIP (section `.rodata`, alignement 4 octets).
- **Consommation mémoire** :
  - **RAM (.data + .bss)** : ~16.8 Ko (sur 1024 Ko disponibles).
  - **Flash (.text + .rodata)** : ~1.26 Mo avec 7 jeux intégrés.
- **Mappers supportés** : NROM (0), MMC1 (1), UNROM (2), CNROM (3), MMC3 (4), MMC5 (5), AOROM (7), GxROM (66), et plus de 30 autres mappers intégrés dans Nofrendo.

---

## 📜 Licence

- **Nofrendo** : GNU General Public License v2 (GPL-2.0).
- **Portage NumWorks** : Logiciel libre sous licence MIT / GPL-2.0.
