# ADRIEN DO IT BETTER — V2

![Aperçu](docs/apercu.png)

Hand-tuned by **Méchant Humain**. Un plugin pour macOS qui fait le Pultec trick dans le grave et sature une bande de fréquences, avec Adrien, ingé son de la rue Doudeauville, qui réagit à chaque réglage.

**Télécharger :** onglet *Releases* (tag v2.x), ou *Actions* → dernier build → artefact `ADRIEN_DO_IT_BETTER_V2.x_macOS` (le numéro de version est dans le nom).
Le zip contient le VST3, l'AAX (Pro Tools, pas encore signé) et **`INSTALLATION.txt`** avec toutes les commandes Terminal : désinstaller la V1, installer la V2, la débloquer, la vérifier.

## Nouveautés de la V2

- FREQUENCY crantée : 30, 40, 50, 60, 80, 100, 150, 200, 250, 300, 350, 400, 450 Hz
- Le mode LOW/HIGH est supprimé (le plugin travaille le grave)
- AUTO GAIN sur la saturation : le volume reste le même quel que soit le DRIVE
- L'écran de courbe d'EQ est supprimé, remplacé par l'afficheur des crans
- Interface dans le style de Phosphor XY : gris clair mat, accent vert, fenêtre redimensionnable
- Adrien en bas à droite, une réaction et une phrase pour chaque réglage (40 phrases)
- Nouveau plugin : « ADRIEN DO IT BETTER V2 », éditeur « Méchant Humain ». La V1 est à désinstaller.

## Les réglages

| Section | Réglage | Ce que ça fait |
|---|---|---|
| PULTEC TRICK | FREQUENCY | 13 crans de 30 à 450 Hz. On peut aussi cliquer sur un cran de l'afficheur. |
| | BOOST (0–10) | Shelf jusqu'à +13,5 dB sous la fréquence |
| | ATTEN (0–10) | Cloche jusqu'à −10 dB, une octave au-dessus. Avec BOOST, ça donne la bosse et le creux du Pultec trick. |
| | EQ DRY / WET | Mélange entre le signal non égalisé et le signal égalisé |
| SATURATION | TAPE / WARM / FUZZ / CRUSH / DIODE | Type de saturation |
| | DRIVE (0–10) | Jusqu'à +30 dB dans la bande. À 0, le signal n'est pas touché. |
| | SAT DRY / WET | Mélange de la bande saturée |
| | AUTO GAIN | Compense le volume de la saturation (±18 dB max), pour comparer à volume égal |
| OUTPUT | OUTPUT | ±18 dB |

Double-clic sur un bouton : retour à la valeur par défaut. La saturation ne touche qu'une bande d'environ 1,7 octave autour de FREQUENCY, en suréchantillonnage ×4 (4 échantillons de latence, déclarés au DAW).

## Adrien

Chaque réglage déclenche une réaction et une phrase dans sa bulle :

| Réglage | Réaction |
|---|---|
| FREQUENCY | Il sursaute à chaque cran |
| BOOST / ATTEN | Il sourit / il fait la moue. Les deux hauts : pouce levé |
| EQ DRY / WET | Main à l'oreille, tête penchée |
| Type de saturation | Sépia, halo, cheveux qui frisent, pixelisation, éclairs |
| DRIVE | Il rougit, transpire, fume puis tremble |
| SAT DRY / WET | Il hoche la tête |
| AUTO GAIN | On : casque et pouce levé. Off : haussement d'épaules |
| OUTPUT | Il recule (mains sur les oreilles au-delà de 0 dB), ou se penche |
| Son fort / clip | Headbang et cornes / grimace et panneau CLIP |
| Pas de son / rien touché 20 s | Il bâille / pause welsh |
| Double-clic | Clin d'œil |

## Compiler (macOS)

1. `xcode-select --install`, puis `brew install cmake`
2. `./build_mac.sh` (JUCE 8.0.4 est téléchargé automatiquement)

Pro Tools : `cmake -S . -B build -DADRIEN_PLUGIN_FORMATS="VST3;AAX"`. Le SDK AAX est fourni avec JUCE 8, mais Pro Tools n'ouvre l'AAX qu'une fois signé avec PACE (compte développeur Avid + compte PACE/iLok).

Tests hors DAW (Linux ou macOS) : `cmake -S . -B build -DADRIEN_BUILD_TESTS=ON -DADRIEN_COPY_AFTER_BUILD=OFF`, puis lancer `AdrienTests` (paramètres, AUTO GAIN, réactions, captures de l'interface).

## Structure

- `Source/Dsp.h` : biquads, SVF, saturations
- `Source/PluginProcessor.*` : paramètres, traitement audio, AUTO GAIN
- `Source/PluginEditor.*` : interface (style Phosphor XY), crans, bulle, déclenchement des réactions
- `Source/BraunLookAndFeel.h` : façade et touches reprises de Phosphor XY, potards façon Pittsburgh Modular
- `Source/AdrienView.*` : Adrien et toutes ses réactions
- `Source/Phrases.h` : les 40 phrases
- `Tests/Tests.cpp` : tests hors DAW
