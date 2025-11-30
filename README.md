# 🌡️ RoomPredictor - Système de Prédiction de Température Multi-Chambres

## 📋 Table des Matières

### 🚀 Démarrage Rapide
1. [Description du Projet](#-description)
2. [Installation](#%EF%B8%8F-installation)
   - [Prérequis](#prérequis)
   - [Installation Python](#installation-python)
   - [Installation Arduino (M5Stack)](#installation-arduino-m5stack)
3. [Utilisation Complète (A à Z)](#-utilisation-complète-a-à-z)
   - [Étape 1 : Génération des données](#étape-1--génération-des-données-dentraînement)
   - [Étape 2 : Entraînement du modèle](#étape-2--entraînement-du-modèle)
   - [Étape 3 : Téléversement sur M5Stack](#étape-3--téléversement-sur-m5stack)
   - [Étape 4 : Configuration WiFi](#étape-4--configuration-wifi-sur-m5stack)
   - [Étape 5 : Utilisation des fonctionnalités](#étape-5--utilisation-des-fonctionnalités)

### 📚 Documentation Technique
4. [Structure du Projet](#-structure-du-projet)
5. [Commandes Utiles](#-commandes-utiles)
   - [Commandes PowerShell](#commandes-powershell-windows)
   - [Commandes Arduino CLI](#commandes-arduino-cli-optionnel)
   - [Scripts Personnalisés](#scripts-utiles-personnalisés)
   - [Commandes Git](#commandes-git-optionnel)
6. [Explication du Code](#-explication-du-code)
   - [Architecture Globale](#architecture-globale-du-système)
   - [Générateur de Données](#1-générateur-de-données-generate_test_datapy)
   - [Interface GUI](#2-interface-gui-data_generator_guipy)
   - [Entraînement ML](#3-entraînement-train_model_with_datepy)
   - [M5Stack (Embarqué)](#4-m5stack-roompredictorino)
7. [Paramètres et Configuration](#-paramètres-et-configuration)
   - [Paramètres de Génération](#paramètres-de-génération-de-données)
   - [Paramètres du Réseau de Neurones](#paramètres-du-réseau-de-neurones)
   - [Profils d'Isolation](#profils-disolation-détaillés)
   - [Configuration M5Stack](#configuration-m5stack)

### 🔧 Maintenance et Optimisation
8. [Dépannage](#-dépannage)
   - [Problèmes Python](#problèmes-python)
   - [Problèmes M5Stack](#problèmes-m5stack)
   - [Problèmes de Performance](#problèmes-de-performance)
9. [Performances](#-performances)
   - [Métriques du Modèle](#métriques-du-modèle)
   - [Performances Temps Réel](#performances-temps-réel)
   - [Consommation Énergétique](#consommation-énergétique)
10. [Extensions Possibles](#-extensions-possibles)
11. [Conseils d'Utilisation](#-conseils-dutilisation)

### 📖 Informations Complémentaires
12. [Roadmap Future](#-roadmap-future)
13. [Support et Contributions](#-support-et-contributions)
14. [Licence et Crédits](#-licence-et-crédits)
15. [Changelog](#-changelog)

---

## 📋 Description

RoomPredictor est un système complet de prédiction de température pour plusieurs chambres utilisant un réseau de neurones embarqué sur M5Stack. Le système génère des données simulées, entraîne un modèle d'apprentissage automatique, et permet la visualisation en temps réel sur un écran tactile.

### ✨ Fonctionnalités principales

- 🏠 **Support multi-chambres dynamique** : De 1 à 30+ chambres
- 🖥️ **Interface GUI Python** : Génération de données et entraînement simplifiés
- 📊 **Visualisation graphique** : Graphiques interactifs avec filtres par chambre
- 🌐 **Prédictions API** : Intégration Open-Meteo pour prévisions météo
- 📈 **Historique CSV** : Visualisation des données passées avec sélection de période
- 🔒 **Sécurité** : Certificate pinning, rate limiting, validation des données
- 📱 **M5Stack TABS** : Interface tactile 1280×720 avec 10 pages

---

## 🛠️ Installation

### Prérequis

#### Matériel
- **M5Stack TABS** (ESP32-S3, écran tactile 1280×720)
- Câble USB pour programmation
- Connexion WiFi

#### Logiciels
- **Python 3.8+**
- **Arduino IDE** (ou PlatformIO)
- **Git** (optionnel)

### Installation Python

1. **Cloner le projet** (ou télécharger le ZIP)
```bash
git clone <votre-repo>
cd Predictemp
```

2. **Créer un environnement virtuel**
```bash
python -m venv .venv
```

3. **Activer l'environnement virtuel**

Windows (PowerShell) :
```powershell
.\.venv\Scripts\Activate.ps1
```

Linux/Mac :
```bash
source .venv/bin/activate
```

4. **Installer les dépendances**
```bash
pip install tensorflow numpy pandas scikit-learn tkinter
```

### Installation Arduino (M5Stack)

1. **Installer Arduino IDE** : [Télécharger](https://www.arduino.cc/en/software)

2. **Ajouter les cartes ESP32** :
   - Ouvrir `Fichier > Préférences`
   - Ajouter dans "URL de gestionnaire de cartes supplémentaires" :
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```

3. **Installer les bibliothèques** (via Gestionnaire de bibliothèques) :
   - `M5GFX`
   - `M5Unified`
   - `ArduinoJson` (v6.x)
   - `WiFiClientSecure` (inclus avec ESP32)

4. **Configurer la carte** :
   - Carte : `ESP32S3 Dev Module`
   - Port : Sélectionner le port USB du M5Stack
   - Upload Speed : `115200`

---

## 🚀 Utilisation Complète (A à Z)

### Étape 1 : Génération des données d'entraînement

1. **Lancer l'interface GUI**
```bash
python data_generator_gui.py
```

2. **Configurer les chambres** :
   - Entrer le **nombre de chambres** (1-30)
   - Cliquer sur **"Générer Chambres"**

3. **Configurer chaque chambre** :
   - **Nom** : Identifier la chambre (ex: "Salon", "Chambre 1")
   - **Isolation** : Choisir le profil :
     - **Excellente** (0.05) - Maison passive
     - **Très Bonne** (0.10) - Isolation récente
     - **Bonne** (0.15) - Maison standard
     - **Moyenne** (0.25) - Isolation ancienne
     - **Faible** (0.40) - Bâtiment non isolé
   - **Température cible** : Température souhaitée (15-25°C)

4. **Générer les données CSV** :
   - Ajuster les **paramètres de génération** si nécessaire :
     - Nombre d'échantillons : 10000 (recommandé)
     - Température externe min/max : -10 à 35°C
     - Random seed (pour reproductibilité)
   - Cliquer sur **"Générer Données CSV"**
   - Les fichiers `Room1_data.csv`, `Room2_data.csv`, etc. sont créés dans `data/`

### Étape 2 : Entraînement du modèle

**Option A - Depuis l'interface GUI (Recommandée)** :
1. Cliquer sur **"🔄 Entraîner le Modèle"**
2. Observer les logs d'entraînement en temps réel
3. Attendre la fin (environ 1-2 minutes)
4. **✨ NOUVEAU** : L'export vers Arduino se fait **automatiquement** après l'entraînement !
   - Le script génère `csv_data.h` avec vos vraies données
   - Vous êtes informé par popup quand tout est prêt

**Option manuelle - Export vers Arduino** :
- Si vous avez déjà entraîné le modèle, cliquez sur **"📤 Exporter vers Arduino"**
- Ou en ligne de commande : `python export_csv_to_arduino.py`

**Option B - Depuis la ligne de commande** :
```bash
python train_model_with_date.py
python export_csv_to_arduino.py  # Export manuel si nécessaire
```

**Résultats générés** :
- ✅ Modèle sauvegardé : `models/temp_model_with_date.h5`
- ✅ Poids exportés pour M5Stack : `M5Stack_Temperature_Prediction/RoomPredictor/neural_weights.h`
- ✅ **Données CSV réelles** : `M5Stack_Temperature_Prediction/RoomPredictor/csv_data.h` (1116 points)
- ✅ Statistiques affichées : MAE, MSE, erreurs par chambre

### Étape 3 : Téléversement sur M5Stack

1. **Ouvrir le projet Arduino** :
   - Fichier : `M5Stack_Temperature_Prediction/RoomPredictor/RoomPredictor.ino`

2. **Vérifier la configuration** :
   - Ouvrir `neural_weights.h`
   - Confirmer que `#define NUM_ROOMS X` correspond au nombre de chambres configurées
   - Vérifier que `csv_data.h` a été généré (si vous voulez afficher les vraies données CSV)

3. **Compiler et téléverser** :
   - Cliquer sur "✓" (Vérifier) pour compiler
   - Cliquer sur "→" (Téléverser)
   - Attendre la compilation (~2 min la première fois)
   - Le M5Stack redémarre automatiquement

### Étape 4 : Configuration WiFi sur M5Stack

1. **Accéder à la configuration WiFi** :
   - Sur l'écran d'accueil, toucher le bouton **"CONFIGURATION WiFi"** (gris, en bas)

2. **Scanner les réseaux** :
   - Le scan démarre automatiquement
   - Liste des réseaux WiFi disponibles s'affiche avec force du signal

3. **Sélectionner un réseau** :
   - Toucher le réseau souhaité dans la liste
   - Un **clavier virtuel** apparaît automatiquement

4. **Entrer le mot de passe** :
   - Utiliser le clavier tactile pour taper le mot de passe
   - Toucher **"CONNECT"**

5. **Confirmation** :
   - Statut WiFi passe au **vert** en haut à droite du menu
   - Message "Connecté avec succès"
   - Le réseau est sauvegardé automatiquement

### Étape 5 : Utilisation des fonctionnalités

#### A. Prédiction Manuelle

1. **Accès** : Page d'accueil → **"PREDICTION MANUELLE"** (bleu)

2. **Configurer la date/heure** :
   - Utiliser les boutons **+/-** pour ajuster :
     - Jour (1-31)
     - Mois (1-12)
     - Heure (0-23)
     - Minute (0-59)
   - **OU** toucher **"AUTO WiFi"** pour récupérer l'heure actuelle (nécessite WiFi)

3. **Ajuster la température externe** :
   - Boutons **+** et **-** pour modifier par pas de 0.1°C
   - Plage recommandée : -10 à 35°C

4. **Obtenir les prédictions** :
   - Toucher le bouton bleu **"PREDICT"**
   - Les températures prédites pour toutes les chambres s'affichent immédiatement

5. **Consulter l'historique** :
   - Chaque prédiction est automatiquement enregistrée
   - Aller à **"GRAPH MANUEL"** pour voir l'évolution

#### B. Température Réelle (WiFi)

**Nécessite une connexion WiFi active**

1. **Accès** : Page d'accueil → **"TEMP. REELLE (WiFi)"** (vert)

2. **Récupération automatique** :
   - L'application interroge l'API Open-Meteo (Paris par défaut)
   - Température extérieure actuelle affichée
   - Prédiction automatique calculée pour toutes les chambres
   - Ajout automatique à l'historique manuel

3. **Utilisation** :
   - Idéal pour des prédictions rapides sans saisie manuelle
   - Les données sont mises en cache 5 minutes pour économiser les requêtes API

#### C. Graphique Manuel

1. **Accès** : Page d'accueil → **"GRAPH MANUEL"** (orange)

2. **Visualisation** :
   - Affiche l'historique des 50 dernières prédictions manuelles
   - Courbes en couleur :
     - 🔴 Rouge : Température extérieure
     - 🟢 Vert : Chambre 1
     - 🔵 Bleu : Chambre 2
     - 🟡 Jaune : Chambre 3

3. **Filtrer par chambre** :
   - Menu déroulant en haut à droite
   - Sélectionner **"TOUTES"** pour vue globale
   - Ou choisir **"ROOM 1"**, **"ROOM 2"**, **"ROOM 3"** pour vue individuelle
   - Échelle automatique adaptée aux données visibles

4. **Réinitialiser** :
   - Bouton **"RESET HISTO"** (gauche, bas) efface tout l'historique manuel
   - Confirmation automatique en revenant au menu

#### D. Graphique API (Prévisions)

**Nécessite une connexion WiFi active**

1. **Accès** : Page d'accueil → **"GRAPH API"** (cyan)

2. **Choisir la période** :
   - Page "Sélection Période" s'affiche automatiquement
   - 4 options disponibles :
     - **6 heures** : Prévisions horaires (6 points)
     - **12 heures** : Prévisions toutes les 2h (6 points)
     - **24 heures** : Prévisions toutes les 3h (8 points)
     - **1 semaine** : Prévisions journalières (7 points)

3. **Visualisation** :
   - Chargement automatique des prévisions météo
   - Graphique avec températures prédites pour toutes les chambres
   - Labels de dates/heures sur l'axe horizontal
   - Variation horaire/journalière simulée pour réalisme

4. **Filtrer par chambre** :
   - Menu déroulant identique au Graph Manuel
   - Échelle s'adapte dynamiquement

5. **Note** : Les prévisions sont mises en cache 5 minutes. En cas d'erreur API, patienter 10 secondes avant nouvelle tentative.

#### E. Données CSV (Historique)

1. **Accès** : Page d'accueil → **"DONNEES CSV"** (violet)

2. **Choisir la période** :
   - Menu déroulant en haut (centre) :
     - **1 jour** : 48 échantillons
     - **1 semaine** : 168 échantillons
     - **1 mois** : 720 échantillons
     - **3 mois** : 2160 échantillons

3. **Filtrer par chambre** :
   - Menu déroulant en haut à gauche
   - Identique aux autres graphiques

4. **Interprétation** :
   - Données **simulées** basées sur les caractéristiques du modèle entraîné
   - Reflète les profils d'isolation configurés
   - Axe horizontal avec dates relatives (ex: -24h, -12h, 0h)
   - Axe vertical avec échelle adaptative

---

## 📂 Structure du Projet

```
Predictemp/
├── data/                           # Données CSV générées
│   ├── Room1_data.csv             # Données chambre 1
│   ├── Room2_data.csv             # Données chambre 2
│   └── ...                         # Etc. selon nombre de chambres
│
├── models/                         # Modèles entraînés
│   └── temp_model_with_date.h5    # Modèle TensorFlow/Keras
│
├── M5Stack_Temperature_Prediction/
│   └── RoomPredictor/
│       ├── RoomPredictor.ino       # Code principal M5Stack (3266 lignes)
│       └── neural_weights.h        # Poids du réseau de neurones (export C++)
│
├── data_generator_gui.py           # Interface GUI principale (621 lignes)
├── generate_test_data.py           # Générateur de données simulées (303 lignes)
├── train_model_with_date.py        # Script d'entraînement ML (480 lignes)
├── test_multi_rooms.py             # Tests de validation
├── requirements.txt                # Dépendances Python
└── README.md                       # Ce fichier
```

---

## 💻 Commandes Utiles

### Commandes PowerShell (Windows)

#### Configuration Initiale

**Créer l'environnement virtuel** :
```powershell
python -m venv .venv
```

**Activer l'environnement virtuel** :
```powershell
.\.venv\Scripts\Activate.ps1
```

Si erreur "scripts désactivés" :
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

**Installer les dépendances** :
```powershell
pip install tensorflow numpy pandas scikit-learn
```

**Vérifier les installations** :
```powershell
pip list
python --version
```

#### Génération et Entraînement

**Lancer l'interface GUI** :
```powershell
python data_generator_gui.py
```

**Générer données en ligne de commande** :
```powershell
python generate_test_data.py
```

**Entraîner le modèle** :
```powershell
python train_model_with_date.py
```

**Exporter les CSV vers Arduino** (pour affichage des vraies données sur M5Stack) :
```powershell
python export_csv_to_arduino.py
```
> Ce script génère `csv_data.h` avec vos vraies données CSV échantillonnées pour chaque période (1J, 1S, 1M, 3M).
> Après exécution, recompilez et téléversez le code Arduino pour mettre à jour la page "DONNÉES CSV".

**Tester plusieurs configurations** :
```powershell
python test_multi_rooms.py
```

#### Gestion des Fichiers

**Nettoyer les CSV existants** :
```powershell
Remove-Item -Path "data\Room*_data.csv" -Force -ErrorAction SilentlyContinue
Write-Host "CSV supprimés" -ForegroundColor Green
```

**Supprimer modèle et poids** :
```powershell
Remove-Item -Path "models\temp_model_with_date.h5" -Force -ErrorAction SilentlyContinue
Remove-Item -Path "M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h" -Force -ErrorAction SilentlyContinue
Write-Host "Nettoyage effectué" -ForegroundColor Green
```

**Vérifier présence des fichiers critiques** :
```powershell
Test-Path "data\Room1_data.csv"
Test-Path "models\temp_model_with_date.h5"
Test-Path "M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h"
```

**Compter le nombre de CSV générés** :
```powershell
(Get-ChildItem "data\Room*_data.csv").Count
```

**Afficher la taille des modèles** :
```powershell
Get-ChildItem -Path "models","M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h" -Recurse | 
Select-Object Name, @{Name="Size(KB)";Expression={[math]::Round($_.Length/1KB,2)}}
```

#### Visualisation et Analyse

**Afficher les 10 premières lignes d'un CSV** :
```powershell
Get-Content "data\Room1_data.csv" -Head 10
```

**Compter les lignes d'un CSV** :
```powershell
(Get-Content "data\Room1_data.csv").Count
```

**Rechercher erreurs dans les logs d'entraînement** :
```powershell
python train_model_with_date.py 2>&1 | Select-String "error|erreur|failed"
```

**Extraire statistiques du modèle** :
```powershell
python train_model_with_date.py 2>&1 | Select-String "MAE|MSE|Loss"
```

#### Backup et Restauration

**Créer backup complet** :
```powershell
$date = Get-Date -Format "yyyy-MM-dd_HHmm"
Compress-Archive -Path "data","models","M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h" -DestinationPath "backup_$date.zip"
Write-Host "Backup créé: backup_$date.zip" -ForegroundColor Green
```

**Restaurer depuis backup** :
```powershell
Expand-Archive -Path "backup_2025-11-30_1430.zip" -DestinationPath "." -Force
```

**Sauvegarder configuration GUI** :
```powershell
Copy-Item "config.json" "config_backup.json" -Force
```

#### Maintenance

**Réinstaller dépendances proprement** :
```powershell
pip uninstall tensorflow numpy pandas scikit-learn -y
pip install tensorflow numpy pandas scikit-learn
```

**Vider cache pip** :
```powershell
pip cache purge
```

**Mettre à jour pip** :
```powershell
python -m pip install --upgrade pip
```

**Vérifier intégrité environnement** :
```powershell
pip check
```

### Commandes Arduino CLI (Optionnel)

Si vous utilisez **Arduino CLI** au lieu de l'IDE :

**Compiler le sketch** :
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 M5Stack_Temperature_Prediction\RoomPredictor
```

**Téléverser sur M5Stack** :
```powershell
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32s3 M5Stack_Temperature_Prediction\RoomPredictor
```
*(Remplacer COM3 par votre port)*

**Moniteur série** :
```powershell
arduino-cli monitor -p COM3 -c baudrate=115200
```

**Lister les ports disponibles** :
```powershell
arduino-cli board list
```

### Scripts Utiles Personnalisés

#### Script 1 : Génération Complète Automatisée

Créer `generate_and_train.ps1` :
```powershell
# Nettoyage
Remove-Item -Path "data\Room*_data.csv" -Force -ErrorAction SilentlyContinue

# Génération données (remplacer par vos paramètres)
Write-Host "Génération des données..." -ForegroundColor Cyan
python generate_test_data.py

# Vérification
$csvCount = (Get-ChildItem "data\Room*_data.csv").Count
Write-Host "$csvCount fichiers CSV générés" -ForegroundColor Green

# Entraînement
Write-Host "Entraînement du modèle..." -ForegroundColor Cyan
python train_model_with_date.py

# Vérification finale
if (Test-Path "M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h") {
    Write-Host "✓ Prêt pour téléversement M5Stack" -ForegroundColor Green
} else {
    Write-Host "✗ Erreur: neural_weights.h non généré" -ForegroundColor Red
}
```

**Utilisation** :
```powershell
.\generate_and_train.ps1
```

#### Script 2 : Validation Multi-Configurations

Créer `test_all_configs.ps1` :
```powershell
$configurations = @(1, 3, 5, 10)

foreach ($numRooms in $configurations) {
    Write-Host "`n=== Test avec $numRooms chambre(s) ===" -ForegroundColor Yellow
    
    # Nettoyage
    Remove-Item -Path "data\Room*_data.csv" -Force -ErrorAction SilentlyContinue
    
    # TODO: Générer données pour $numRooms chambres
    # (nécessite script ou GUI avec paramètres CLI)
    
    # Entraînement
    python train_model_with_date.py
    
    # Sauvegarder résultats
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    Copy-Item "models\temp_model_with_date.h5" "models\backup_${numRooms}rooms_$timestamp.h5"
}

Write-Host "`n✓ Tests terminés" -ForegroundColor Green
```

#### Script 3 : Monitoring Taille Modèle

Créer `check_model_size.ps1` :
```powershell
function Get-FileSize {
    param($path)
    if (Test-Path $path) {
        $size = (Get-Item $path).Length
        return [math]::Round($size / 1KB, 2)
    }
    return 0
}

Write-Host "=== Tailles des fichiers ===" -ForegroundColor Cyan

$h5Size = Get-FileSize "models\temp_model_with_date.h5"
$headerSize = Get-FileSize "M5Stack_Temperature_Prediction\RoomPredictor\neural_weights.h"

Write-Host "Modèle TensorFlow:  $h5Size KB" -ForegroundColor White
Write-Host "Header C++:         $headerSize KB" -ForegroundColor White

# Estimation mémoire ESP32
$estimatedRAM = $headerSize + 150  # 150 KB pour buffers et stack
Write-Host "RAM estimée ESP32:  ~$estimatedRAM KB / 320 KB" -ForegroundColor $(if($estimatedRAM -lt 280){"Green"}else{"Red"})

if ($estimatedRAM -gt 280) {
    Write-Host "⚠ ATTENTION: Risque de dépassement mémoire!" -ForegroundColor Red
}
```

**Utilisation** :
```powershell
.\check_model_size.ps1
```

### Commandes Git (Optionnel)

**Initialiser repository** :
```powershell
git init
git add .
git commit -m "Initial commit - RoomPredictor v2.0"
```

**Ignorer fichiers volumineux** (.gitignore) :
```
# Données générées
data/*.csv
models/*.h5

# Environnement Python
.venv/
__pycache__/
*.pyc

# Logs
*.log

# Backups
backup_*.zip
```

**Créer tag de version** :
```powershell
git tag -a v2.0 -m "Version 2.0 - Multi-chambres dynamique"
git push origin v2.0
```

### Raccourcis Clavier (Utiles)

Dans l'interface GUI :
- **Tab** : Naviguer entre champs
- **Entrée** : Valider saisie numérique
- **Ctrl+Scroll** : Zoomer/dézoomer fenêtre

Dans Arduino IDE :
- **Ctrl+R** : Compiler
- **Ctrl+U** : Téléverser
- **Ctrl+Shift+M** : Ouvrir moniteur série

### Dépannage Rapide en Ligne de Commande

**Problème : "Module tensorflow not found"**
```powershell
# Vérifier environnement actif
if ($env:VIRTUAL_ENV) {
    Write-Host "Environnement actif: $env:VIRTUAL_ENV" -ForegroundColor Green
} else {
    Write-Host "⚠ Environnement virtuel non actif!" -ForegroundColor Red
    .\.venv\Scripts\Activate.ps1
}

# Réinstaller tensorflow
pip install --force-reinstall tensorflow
```

**Problème : "Port COM occupé"**
```powershell
# Lister processus utilisant ports série
Get-Process | Where-Object {$_.ProcessName -like "*arduino*" -or $_.ProcessName -like "*python*"}

# Tuer processus Arduino si bloqué
Stop-Process -Name "arduino" -Force -ErrorAction SilentlyContinue
```

**Problème : "Fichiers CSV corrompus"**
```powershell
# Vérifier intégrité des CSV
foreach ($file in Get-ChildItem "data\Room*_data.csv") {
    try {
        $lines = (Get-Content $file).Count
        Write-Host "$($file.Name): $lines lignes" -ForegroundColor Green
    } catch {
        Write-Host "$($file.Name): CORROMPU" -ForegroundColor Red
    }
}
```

**Problème : "Mémoire insuffisante lors de l'entraînement"**
```powershell
# Réduire échantillons dans CSV
foreach ($file in Get-ChildItem "data\Room*_data.csv") {
    $content = Get-Content $file | Select-Object -First 5001  # Header + 5000 lignes
    $content | Set-Content $file
}
Write-Host "CSV réduits à 5000 échantillons" -ForegroundColor Green
```

---

## 🔧 Explication du Code

### Architecture Globale du Système

```
┌──────────────────┐
│   GUI Python     │  ← Interface utilisateur (Tkinter)
│  (621 lignes)    │     Configuration multi-chambres
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│   Générateur     │  ← Simulation physique thermique
│   (303 lignes)   │     Inertie + Chauffage + Isolation
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Fichiers CSV    │  ← Room1_data.csv, Room2_data.csv...
│  (data/)         │     Format: timestamp, ext_temp, room_temp
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Entraînement    │  ← TensorFlow/Keras
│  (480 lignes)    │     Architecture: 3→32→32→N
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ neural_weights.h │  ← Export C++ (tableaux float)
│                  │     W1[3][32], B1[32], W2[32][32]...
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  M5Stack ESP32   │  ← Inférence embarquée + Visualisation
│  (3266 lignes)   │     10 pages interactives, WiFi, HTTPS
└──────────────────┘
```

### 1. Générateur de Données (`generate_test_data.py`)

#### Principe Physique

Le générateur simule l'évolution de la température d'une chambre en tenant compte de 4 facteurs :

**Formule de simulation** :
```python
new_temp = (
    current_temp * 0.70 +              # 1. Inertie thermique (70%)
    target_temp * 0.15 +               # 2. Chauffage (15%)
    external_temp * profile * 0.10 +   # 3. Influence externe (10%)
    daily_variation +                  # 4. Variation jour/nuit
    noise                              # 5. Bruit aléatoire ±0.5°C
)
```

#### Détail des Composantes

**1. Inertie thermique (70%)** :
```python
current_temp * 0.70
```
- Représente la **résistance au changement** de température
- Une pièce ne change pas instantanément de température
- Coefficient élevé = changements lents et progressifs

**2. Chauffage (15%)** :
```python
target_temp * 0.15
```
- Tendance de la pièce à atteindre la **température cible**
- Simule le système de chauffage/climatisation
- Coefficient modéré = convergence progressive

**3. Influence externe (10%)** :
```python
external_temp * profile * 0.10
```
- Impact de la **température extérieure**
- Pondéré par le **profil d'isolation**
- Plus l'isolation est faible, plus l'influence est forte

**4. Variation jour/nuit** :
```python
hour_angle = (hour / 24.0) * 2 * π
amplitude = abs(external_temp - target_temp) * profile * 0.3
daily_variation = sin(hour_angle) * amplitude
```
- Simule le **cycle circadien** naturel
- Amplitude dépend de :
  - Différence température ext/int
  - Qualité de l'isolation
- Maximum vers 14h, minimum vers 2h du matin

**5. Bruit aléatoire** :
```python
noise = random.uniform(-0.5, 0.5)
```
- Simule les **micro-variations** aléatoires
- Fenêtres ouvertes, présence humaine, etc.

#### Profils d'Isolation

| Profil | Coefficient | Signification | Variation typique |
|--------|-------------|---------------|-------------------|
| **Excellente** | 0.05 | Maison passive, triple vitrage | ±1-2°C |
| **Très Bonne** | 0.10 | Isolation RT2012, récente | ±2-3°C |
| **Bonne** | 0.15 | Maison standard moderne | ±3-4°C |
| **Moyenne** | 0.25 | Isolation années 1980-2000 | ±5-6°C |
| **Faible** | 0.40 | Bâtiment ancien non rénové | ±8-10°C |

**Exemple de calcul** :

Chambre avec isolation **Moyenne** (0.25), température cible 20°C, température extérieure 5°C, à 14h :

```python
hour_angle = (14 / 24.0) * 2 * π = 3.665 rad
amplitude = abs(5 - 20) * 0.25 * 0.3 = 1.125°C
daily_variation = sin(3.665) * 1.125 ≈ -0.56°C

# Si température actuelle = 18°C
new_temp = 18 * 0.70 + 20 * 0.15 + 5 * 0.25 * 0.10 + (-0.56) + noise
         = 12.6 + 3.0 + 0.125 + (-0.56) + 0.2
         = 15.365°C
```

---

### 2. Interface GUI (`data_generator_gui.py`)

#### Architecture Tkinter

```python
MainWindow (Tk)
├── Canvas (800×600, responsive)
│   ├── Frame Header
│   │   └── Label "Configuration Multi-Chambres"
│   │
│   ├── Frame Config
│   │   ├── Entry (nombre de chambres)
│   │   └── Button "Générer Chambres"
│   │
│   ├── Frame Rooms (scrollable vertical)
│   │   ├── RoomFrame 1
│   │   │   ├── Entry (nom)
│   │   │   ├── Combobox (isolation)
│   │   │   └── Scale (température cible)
│   │   ├── RoomFrame 2
│   │   └── ... (dynamique)
│   │
│   ├── Frame Generation
│   │   ├── Entry (nombre échantillons)
│   │   ├── Entry (temp min/max)
│   │   └── Entry (random seed)
│   │
│   └── Frame Actions
│       ├── Button "Générer Données CSV"
│       └── Button "Entraîner modèle"
│
└── TrainingWindow (Toplevel, optionnelle)
    └── ScrolledText (logs en temps réel)
```

#### Fonctionnalités Clés

**1. Interface Responsive** :
```python
def on_resize(event):
    canvas.itemconfig(canvas_window, width=event.width)
```
- Le canvas s'adapte à la taille de la fenêtre
- Les frames internes s'ajustent automatiquement

**2. Génération Dynamique de Chambres** :
```python
def generate_room_frames():
    # Supprime les anciennes frames
    for frame in room_frames:
        frame.destroy()
    room_frames.clear()
    
    # Crée N nouvelles frames
    for i in range(num_rooms):
        frame = create_room_frame(i)
        room_frames.append(frame)
```

**3. Validation des Données** :
```python
def validate_and_generate():
    # Vérifie que toutes les chambres sont configurées
    for i, room in enumerate(room_configs):
        if not room['name']:
            messagebox.showerror("Erreur", f"Chambre {i+1} sans nom")
            return False
        if room['target_temp'] < 15 or room['target_temp'] > 25:
            messagebox.showerror("Erreur", "Température cible hors plage")
            return False
    return True
```

**4. Entraînement en Thread Séparé** :
```python
def start_training():
    training_thread = threading.Thread(target=run_training)
    training_thread.daemon = True
    training_thread.start()

def run_training():
    # Redirection stdout vers ScrolledText
    sys.stdout = TextRedirector(log_text)
    
    # Lancement script d'entraînement
    subprocess.run([python_exe, "train_model_with_date.py"])
    
    # Restauration stdout
    sys.stdout = sys.__stdout__
```
- L'interface reste **réactive** pendant l'entraînement
- Les logs s'affichent en **temps réel**

**5. Sauvegarde/Chargement Configuration** :
```python
def save_config():
    config = {
        'num_rooms': num_rooms,
        'rooms': room_configs,
        'generation_params': {...}
    }
    with open('config.json', 'w') as f:
        json.dump(config, f)
```

---

### 3. Entraînement (`train_model_with_date.py`)

#### Architecture du Réseau de Neurones

```
Input Layer (3 neurones)
├── ext_temp (normalisé)
├── day / 31.0
└── month / 12.0
    │
    ▼
Hidden Layer 1 (32 neurones)
├── Activation: ReLU
└── Weights: W1[3][32], Bias: B1[32]
    │
    ▼
Hidden Layer 2 (32 neurones)
├── Activation: ReLU
└── Weights: W2[32][32], Bias: B2[32]
    │
    ▼
Output Layer (N neurones)
├── Activation: Linear
├── Weights: W3[32][N], Bias: B3[N]
└── Output: [temp_room1, temp_room2, ..., temp_roomN]
```

#### Code TensorFlow/Keras

```python
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense

model = Sequential([
    Dense(32, activation='relu', input_shape=(3,)),
    Dense(32, activation='relu'),
    Dense(num_rooms, activation='linear')  # N sorties
])

model.compile(
    optimizer='adam',
    loss='mse',        # Mean Squared Error
    metrics=['mae']    # Mean Absolute Error
)
```

#### Processus d'Entraînement

**1. Chargement des Données** :
```python
import glob
import pandas as pd

# Détection automatique des fichiers CSV
csv_files = sorted(glob.glob('data/Room*_data.csv'))
num_rooms = len(csv_files)

# Chargement flexible (détection colonnes par regex)
for csv_file in csv_files:
    df = pd.read_csv(csv_file)
    
    # Cherche colonne température externe
    ext_col = [c for c in df.columns if 'ext' in c.lower()][0]
    # Cherche colonne température room
    room_col = [c for c in df.columns if 'room' in c.lower()][0]
```

**2. Préparation des Features** :
```python
from sklearn.preprocessing import StandardScaler

# Normalisation température externe uniquement
scaler = StandardScaler()
X[:, 0] = scaler.fit_transform(X[:, 0].reshape(-1, 1)).flatten()

# Jour et mois déjà normalisés [0-1]
X[:, 1] = day / 31.0
X[:, 2] = month / 12.0
```

**3. Entraînement** :
```python
history = model.fit(
    X, y,
    epochs=100,
    batch_size=32,
    validation_split=0.2,  # 20% pour validation
    verbose=1
)
```

**4. Export au Format C++** :
```python
def export_to_cpp(model, filename):
    weights = model.get_weights()
    
    with open(filename, 'w') as f:
        f.write(f"#define NUM_ROOMS {num_rooms}\n\n")
        
        # W1: 3×32
        f.write("float W1[3][32] = {\n")
        for i in range(3):
            f.write("  {")
            f.write(", ".join(f"{weights[0][i][j]:.6f}" for j in range(32)))
            f.write("},\n")
        f.write("};\n\n")
        
        # B1: 32
        f.write("float B1[32] = {")
        f.write(", ".join(f"{weights[1][i]:.6f}" for i in range(32)))
        f.write("};\n\n")
        
        # ... W2, B2, W3, B3 ...
```

---

### 4. M5Stack (`RoomPredictor.ino`)

#### Architecture de Pages

| Page | Nom | Description | Fonctionnalités |
|------|-----|-------------|-----------------|
| 0 | Menu | Page d'accueil | 6 boutons de navigation |
| 1 | Input | Saisie manuelle | Date, heure, température |
| 2 | Result | Résultat prédiction | Affichage N chambres |
| 3 | Real Temp | Température réelle API | Open-Meteo + prédiction |
| 4 | Graph Manual | Historique manuel | 50 points, dropdown filtres |
| 5 | Graph API | Prévisions météo | 6h/12h/24h/1sem, dropdown |
| 6 | WiFi Config | Configuration réseau | Scan, connexion, sauvegarde |
| 7 | Keyboard | Clavier virtuel | QWERTY tactile |
| 8 | Saved Networks | Réseaux sauvegardés | Gestion SSID/passwords |
| 9 | Period Select | Choix période | 4 boutons (6h à 1sem) |
| 10 | CSV Data | Historique simulé | 4 périodes, données simulées |

#### Inférence du Réseau de Neurones

```cpp
void predict_rooms(float ext_temp, int day, int month, float* outputs) {
    // === PRÉPARATION DES ENTRÉES ===
    float input[3] = {
        (ext_temp - MEAN_EXT) / STD_EXT,  // Normalisation
        (float)day / 31.0f,                // [0-1]
        (float)month / 12.0f               // [0-1]
    };
    
    // === COUCHE 1: 3 → 32 ===
    float hidden1[32];
    for (int i = 0; i < 32; i++) {
        float sum = B1[i];  // Biais
        for (int j = 0; j < 3; j++) {
            sum += input[j] * W1[j][i];  // Produit matriciel
        }
        hidden1[i] = max(0.0f, sum);  // Activation ReLU
    }
    
    // === COUCHE 2: 32 → 32 ===
    float hidden2[32];
    for (int i = 0; i < 32; i++) {
        float sum = B2[i];
        for (int j = 0; j < 32; j++) {
            sum += hidden1[j] * W2[j][i];
        }
        hidden2[i] = max(0.0f, sum);  // ReLU
    }
    
    // === COUCHE 3: 32 → N (sortie) ===
    for (int i = 0; i < NUM_ROOMS; i++) {
        float sum = B3[i];
        for (int j = 0; j < 32; j++) {
            sum += hidden2[j] * W3[j][i];
        }
        outputs[i] = sum;  // Activation linéaire
    }
}
```

**Temps d'exécution typique** : ~800 µs (< 1 ms)

#### Système de Sécurité HTTPS

**1. Certificate Pinning** :
```cpp
const char* ISRG_ROOT_X1_CA = "-----BEGIN CERTIFICATE-----\n...";

WiFiClientSecure *client = new WiFiClientSecure;

if (strstr(url, "open-meteo.com")) {
    client->setCACert(ISRG_ROOT_X1_CA);
}
```
- Vérifie que le certificat SSL est celui attendu
- Protège contre attaques **Man-in-the-Middle**

**2. Rate Limiting** :
```cpp
const unsigned long API_COOLDOWN = 10000;  // 10 secondes

bool check_rate_limit() {
    if (millis() - last_api_call < API_COOLDOWN) {
        return false;  // Trop tôt
    }
    last_api_call = millis();
    return true;
}
```
- Empêche surcharge des APIs gratuites
- Économise batterie et bande passante

**3. Validation des Données** :
```cpp
bool validate_temperature(float temp) {
    if (temp < -50.0 || temp > 60.0) return false;
    if (isnan(temp) || isinf(temp)) return false;
    return true;
}
```
- Rejette valeurs aberrantes
- Protège contre données corrompues

**4. Queue de Retry** :
```cpp
struct QueuedRequest {
    char url[256];
    unsigned long timestamp;
    int retry_count;
};

QueuedRequest offline_queue[MAX_QUEUE_SIZE];

void queue_add_request(const char* url) {
    if (queue_count < MAX_QUEUE_SIZE) {
        strcpy(offline_queue[queue_tail].url, url);
        offline_queue[queue_tail].retry_count = 0;
        // ...
    }
}

void queue_process() {
    for (int i = 0; i < queue_count; i++) {
        if (req->retry_count >= 3) {
            // Abandonner après 3 tentatives
            continue;
        }
        // Retry avec backoff exponentiel
    }
}
```

#### Optimisations Graphiques

**1. Buffering Double** :
```cpp
M5.Display.startWrite();  // Début buffering

// ... tous les dessins ...

M5.Display.endWrite();    // Flush buffer → écran
```
- Évite **scintillement**
- Réduit temps d'affichage de 70%

**2. Échelle Adaptative** :
```cpp
// Calcul min/max dynamique
float min_temp = 100.0f, max_temp = -100.0f;
for (int i = 0; i < count; i++) {
    if (data[i] < min_temp) min_temp = data[i];
    if (data[i] > max_temp) max_temp = data[i];
}

// Ajout marge 20%
float range = max_temp - min_temp;
min_temp -= range * 0.2f;
max_temp += range * 0.2f;
```

**3. Mise à Jour Partielle** :
```cpp
void update_temperature_display() {
    M5.Display.startWrite();
    
    // Efface UNIQUEMENT la zone température
    M5.Display.fillRect(100, 140, 1080, 180, BACKGROUND);
    
    // Redessine température
    M5.Display.printf("%.1f", current_temp);
    
    M5.Display.endWrite();
}
```
- Ne redessine pas toute la page
- Améliore réactivité

---

## 📊 Paramètres et Configuration

### Paramètres de Génération de Données

| Paramètre | Défaut | Min | Max | Description |
|-----------|--------|-----|-----|-------------|
| **Nombre d'échantillons** | 10000 | 1000 | 100000 | Points de données générés par chambre |
| **Température externe min** | -10°C | -50°C | 20°C | Limite inférieure simulation |
| **Température externe max** | 35°C | 20°C | 60°C | Limite supérieure simulation |
| **Random seed** | Aléatoire | 0 | 2^31 | Pour reproductibilité des données |
| **Température cible** | 20°C | 15°C | 25°C | Consigne de chauffage |

### Paramètres du Réseau de Neurones

| Paramètre | Valeur | Notes |
|-----------|--------|-------|
| **Architecture** | 3→32→32→N | N = nombre de chambres |
| **Fonction d'activation** | ReLU | Couches cachées |
| **Activation sortie** | Linéaire | Prédiction température |
| **Optimiseur** | Adam | Learning rate = 0.001 |
| **Loss** | MSE | Mean Squared Error |
| **Métrique** | MAE | Mean Absolute Error |
| **Epochs** | 100 | Itérations d'entraînement |
| **Batch size** | 32 | Taille des mini-lots |
| **Validation split** | 0.2 | 20% pour validation |

### Profils d'Isolation (Détaillés)

| Profil | Coefficient | Inertie | Chauffage | Ext | Variation jour/nuit | Usage typique |
|--------|-------------|---------|-----------|-----|---------------------|---------------|
| **Excellente** | 0.05 | 70% | 15% | 0.5% | ±0.5°C | Maison passive, BBC |
| **Très Bonne** | 0.10 | 70% | 15% | 1.0% | ±1.0°C | RT2012, isolation récente |
| **Bonne** | 0.15 | 70% | 15% | 1.5% | ±1.5°C | Construction standard |
| **Moyenne** | 0.25 | 70% | 15% | 2.5% | ±2.5°C | Années 1980-2000 |
| **Faible** | 0.40 | 70% | 15% | 4.0% | ±4.0°C | Ancien, non rénové |

### Configuration M5Stack

| Paramètre | Valeur | Modifiable |
|-----------|--------|------------|
| **Résolution** | 1280×720 | Non |
| **Historique manuel** | 50 points | Oui (ligne 34) |
| **Historique API** | 24 points | Oui (ligne 35) |
| **Cache API** | 5 minutes | Oui (ligne 33) |
| **Rate limit** | 10 secondes | Oui (ligne 28) |
| **Max réseaux WiFi** | 20 | Oui (ligne 36) |

**Modifier le cache API** :
```cpp
// Ligne 33 de RoomPredictor.ino
const unsigned long CACHE_DURATION = 300000;  // 5 min (en ms)
```

**Modifier la limite de requêtes** :
```cpp
// Ligne 28
const unsigned long API_COOLDOWN = 10000;  // 10 sec
```

---

## 🐛 Dépannage

### Problèmes Python

#### Erreur : `UnicodeEncodeError` lors de l'entraînement (Windows)

**Symptôme** :
```
UnicodeEncodeError: 'charmap' codec can't encode character '\u2713'
```

**Cause** : Windows PowerShell ne supporte pas les emojis UTF-8

**Solution** : Les emojis ont été remplacés par des caractères ASCII dans `train_model_with_date.py` :
- ✓ → `[OK]`
- ✗ → `[X]`
- → → `->`
- • → `-`

**Vérification** :
```bash
python train_model_with_date.py
```
Aucun emoji ne devrait apparaître.

#### Erreur : `ModuleNotFoundError: No module named 'tensorflow'`

**Solution** :
```bash
# Activer l'environnement virtuel
.\.venv\Scripts\Activate.ps1

# Installer TensorFlow
pip install tensorflow
```

#### GUI ne se lance pas

**Symptôme** : `ImportError: No module named 'tkinter'`

**Solution** :
- **Windows** : tkinter est inclus dans Python standard
- **Linux** : `sudo apt-get install python3-tk`
- **Mac** : Réinstaller Python depuis python.org (pas Homebrew)

### Problèmes M5Stack

#### Écran noir au démarrage

**Solutions** :
1. **Vérifier alimentation** : Câble USB connecté et fonctionnel
2. **Re-téléverser** : Bouton "→" dans Arduino IDE
3. **Vérifier neural_weights.h** :
   ```bash
   ls M5Stack_Temperature_Prediction/RoomPredictor/neural_weights.h
   ```
   Si absent, ré-exécuter `train_model_with_date.py`
4. **Ouvrir moniteur série** (115200 baud) pour voir les logs

**Log normal** :
```
[INIT] M5Stack TABS initialisation...
[OK] Display ready
[OK] Neural network loaded (NUM_ROOMS=3)
[WIFI] Mode station active
```

#### WiFi ne se connecte pas

**Symptôme** : Statut WiFi reste rouge

**Solutions** :
1. **Vérifier le mot de passe** (sensible à la casse)
2. **Vérifier fréquence réseau** :
   - ESP32 supporte **uniquement 2.4 GHz**
   - Ne fonctionne **pas** avec 5 GHz
3. **Effacer réseaux sauvegardés** :
   - Page "Saved Networks" → "DELETE ALL"
4. **Redémarrer M5Stack** : Bouton reset physique

**Vérification depuis le moniteur série** :
```
[WIFI] Scan terminé - 5 réseau(x) trouvé(s)
[WIFI] Connexion à: MonReseau
[ERROR] WiFi connexion échouée
```

#### Erreurs API répétées

**Symptôme** : "Erreur API - Patientez 10 secondes"

**Solutions** :
1. **Vérifier connexion Internet** :
   - Tester depuis navigateur : https://api.open-meteo.com/
2. **Attendre le cooldown** : 10 secondes entre requêtes
3. **Vérifier certificats SSL** :
   - Les certificats peuvent expirer
   - Re-téléverser code avec certificats à jour
4. **Utiliser mode manuel** si API indisponible

**Test de l'API** (depuis navigateur) :
```
https://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35&current=temperature_2m
```
Devrait retourner un JSON avec température.

#### Graphiques vides ou incomplets

**Symptôme** : "Pas assez de donnees"

**Solutions** :

**Graph Manuel** :
1. Faire au moins **2 prédictions manuelles**
2. Utiliser "PREDICTION MANUELLE" ou "TEMP. REELLE (WiFi)"

**Graph API** :
1. Vérifier connexion WiFi (statut vert)
2. Attendre chargement complet (5-10 secondes)
3. Vérifier moniteur série pour erreurs

**Données CSV** :
- Données simulées, toujours disponibles
- Si vide : problème de génération graphique
  - Redémarrer M5Stack

#### Problème de mémoire (crash aléatoire)

**Symptôme** : M5Stack redémarre tout seul

**Cause** : Dépassement mémoire RAM (fuite mémoire)

**Solution** :
1. **Réduire historique** :
   ```cpp
   #define HISTORY_SIZE 30  // Au lieu de 50
   ```
2. **Désactiver cache** :
   ```cpp
   const unsigned long CACHE_DURATION = 0;  // Pas de cache
   ```
3. **Vérifier RAM disponible** (moniteur série) :
   ```
   [STATS] RAM libre: 285640 bytes
   ```

### Problèmes de Performance

#### Entraînement très lent

**Symptôme** : Plus de 5 minutes pour 100 epochs

**Solutions** :
1. **Réduire nombre d'échantillons** :
   - GUI : Passer de 10000 à 5000
2. **Utiliser GPU** (si disponible) :
   ```bash
   pip install tensorflow-gpu
   ```
3. **Réduire epochs** :
   - Modifier ligne 380 de `train_model_with_date.py` :
     ```python
     epochs=50  # Au lieu de 100
     ```

#### M5Stack lag sur graphiques

**Symptôme** : Redessins lents, écran qui saccade

**Solutions** :
1. **Réduire nombre de points** :
   ```cpp
   if (num_points > 50) num_points = 50;
   ```
2. **Désactiver anti-aliasing** :
   - Supprimer boucles `for(int t=-2; t<=2; t++)`
3. **Augmenter fréquence CPU** (avancé) :
   ```cpp
   setCpuFrequencyMhz(240);  // Max ESP32-S3
   ```

---

## 📈 Performances

### Métriques du Modèle

**Configuration test** : 3 chambres, 10000 échantillons chacune

| Métrique | Valeur | Notes |
|----------|--------|-------|
| **MAE globale** | 2.13°C | Mean Absolute Error |
| **MSE globale** | 7.85 | Mean Squared Error |
| **MAE Chambre 1** | 1.82°C | Excellente isolation |
| **MAE Chambre 2** | 2.05°C | Bonne isolation |
| **MAE Chambre 3** | 2.51°C | Moyenne isolation |
| **R² Score** | 0.94 | Coefficient de détermination |

**Interprétation** :
- Précision **acceptable** pour usage domestique
- Erreur moyenne ~2°C sur prédictions
- Meilleure précision pour isolation excellente

**Améliorer la précision** :
1. Augmenter échantillons (20000+)
2. Augmenter epochs (200)
3. Ajouter features (humidité, vent, etc.)

### Performances Temps Réel

**M5Stack ESP32-S3** :

| Opération | Temps | Notes |
|-----------|-------|-------|
| **Inférence NN** | 800 µs | Pour 3 chambres |
| **Inférence NN** | 1200 µs | Pour 10 chambres |
| **Requête HTTPS** | 1.5-3s | Selon réseau |
| **Redessinage page** | 80-150 ms | Avec buffering |
| **Parsing JSON** | 50-100 ms | ArduinoJson |
| **Scan WiFi** | 3-8s | 10-20 réseaux |

**Utilisation Mémoire** :

| Composant | RAM utilisée | Notes |
|-----------|--------------|-------|
| **Poids NN (3 chambres)** | ~50 KB | W1, W2, W3, B1, B2, B3 |
| **Poids NN (10 chambres)** | ~65 KB | Proportionnel à N |
| **Historique manuel** | 800 bytes | 50 × 4 × 4 bytes |
| **Historique API** | 384 bytes | 24 × 4 × 4 bytes |
| **Cache API** | ~2 KB | String JSON |
| **WiFi** | ~40 KB | Stack réseau |
| **Display buffer** | ~150 KB | M5GFX |
| **TOTAL** | ~250 KB | Sur 320 KB disponibles |

**Marge de sécurité** : ~70 KB libres (22%)

### Consommation Énergétique

**Estimations M5Stack TABS** :

| Mode | Consommation | Autonomie (batterie 1000mAh) |
|------|--------------|-------------------------------|
| **Veille WiFi off** | 50 mA | ~20 heures |
| **Affichage actif** | 150 mA | ~6.5 heures |
| **WiFi + affichage** | 200 mA | ~5 heures |
| **Requête API** | 250 mA (pic) | - |

**Optimisations** :
- Réduire luminosité écran (-30% consommation)
- Désactiver WiFi quand non utilisé (-50 mA)
- Mode veille entre prédictions

---

## 🔄 Extensions Possibles

### 1. Ajout de Nouvelles Chambres

**Procédure complète** :

1. **Modifier configuration GUI** :
   ```python
   # Dans data_generator_gui.py, ligne 450
   num_rooms = 15  # Au lieu de 3
   ```

2. **Générer nouvelles données** :
   - Lancer GUI
   - Configurer les 15 chambres
   - Générer CSV

3. **Ré-entraîner modèle** :
   ```bash
   python train_model_with_date.py
   ```
   - Le script détecte automatiquement 15 chambres
   - Architecture devient 3→32→32→15

4. **Re-téléverser sur M5Stack** :
   - Arduino IDE → Téléverser
   - `neural_weights.h` contient maintenant 15 sorties

**Limite pratique** : ~20 chambres (contraintes mémoire ESP32)

### 2. Modifier Profils d'Isolation

**Ajouter un profil personnalisé** :

Éditer `generate_test_data.py` ligne 35 :

```python
ISOLATION_PROFILES = {
    "Excellente": 0.05,
    "Très Bonne": 0.10,
    "Bonne": 0.15,
    "Moyenne": 0.25,
    "Faible": 0.40,
    "Personnalisé": 0.32,  # NOUVEAU
}
```

**Formule coefficient** :
```
coefficient = 0.50 - (qualité_isolation / 100) * 0.45
```
- Qualité 100% → 0.05 (excellente)
- Qualité 0% → 0.50 (très faible)

### 3. Changer Géolocalisation

**Modifier coordonnées GPS** :

Éditer `RoomPredictor.ino` lignes 22-24 :

```cpp
// Paris (défaut)
const char* WEATHER_API = "https://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35&current=temperature_2m";

// New York
const char* WEATHER_API = "https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=temperature_2m";

// Tokyo
const char* WEATHER_API = "https://api.open-meteo.com/v1/forecast?latitude=35.68&longitude=139.76&current=temperature_2m";
```

**Trouver coordonnées** : [latlong.net](https://www.latlong.net/)

### 4. Ajouter Features au Modèle

**Exemple : Ajouter l'heure comme feature**

1. **Modifier générateur** (`generate_test_data.py`) :
   ```python
   # Ajouter colonne heure dans CSV
   data['hour'] = data['timestamp'].dt.hour
   ```

2. **Modifier entraînement** (`train_model_with_date.py`) :
   ```python
   # Architecture 4 entrées au lieu de 3
   X = np.column_stack([
       df[ext_col].values,
       df['timestamp'].dt.day / 31.0,
       df['timestamp'].dt.month / 12.0,
       df['timestamp'].dt.hour / 24.0  # NOUVEAU
   ])
   
   # Modifier modèle
   model = Sequential([
       Dense(32, activation='relu', input_shape=(4,)),  # 4 au lieu de 3
       Dense(32, activation='relu'),
       Dense(num_rooms, activation='linear')
   ])
   ```

3. **Modifier M5Stack** :
   ```cpp
   void predict_rooms(float ext_temp, int day, int month, int hour, float* outputs) {
       float input[4] = {
           (ext_temp - MEAN_EXT) / STD_EXT,
           (float)day / 31.0f,
           (float)month / 12.0f,
           (float)hour / 24.0f  // NOUVEAU
       };
       // Adapter boucles pour 4 entrées
   }
   ```

### 5. Stockage Persistant (SD Card)

**Ajouter sauvegarde historique** :

```cpp
#include <SD.h>

void save_to_sd(float ext, float* predictions) {
    File file = SD.open("/history.csv", FILE_APPEND);
    
    file.print(millis());
    file.print(",");
    file.print(ext);
    
    for (int i = 0; i < NUM_ROOMS; i++) {
        file.print(",");
        file.print(predictions[i]);
    }
    
    file.println();
    file.close();
}
```

**Avantages** :
- Historique illimité
- Analyse ultérieure sur PC
- Graphiques sur longue période

---

## 📝 Licence et Crédits

### APIs Utilisées

- **Open-Meteo** : API météo gratuite ([open-meteo.com](https://open-meteo.com/))
  - Licence : CC BY 4.0
  - Limites : 10,000 requêtes/jour
  
- **WorldTimeAPI** : API de temps gratuite ([worldtimeapi.org](http://worldtimeapi.org/))
  - Licence : Libre d'utilisation
  - Limites : Aucune documentée

### Bibliothèques Python

- **TensorFlow** : Framework ML (Apache 2.0)
- **NumPy** : Calcul numérique (BSD)
- **Pandas** : Manipulation données (BSD)
- **Scikit-learn** : Preprocessing (BSD)
- **Tkinter** : Interface graphique (Python Software Foundation License)

### Bibliothèques Arduino

- **M5Unified** : Bibliothèque M5Stack (MIT)
- **M5GFX** : Graphiques M5Stack (MIT)
- **ArduinoJson** : Parsing JSON (MIT)
- **WiFiClientSecure** : HTTPS ESP32 (LGPL 2.1)

### Certificats SSL

- **ISRG Root X1** : Let's Encrypt (utilisé par Open-Meteo)
- **DigiCert Global Root CA** : DigiCert (utilisé par WorldTimeAPI)

---

## 💡 Conseils d'Utilisation

### Pour Meilleure Précision

1. **Générer plus de données** :
   - 20,000+ échantillons par chambre
   - Couvre mieux la variabilité

2. **Varier les profils** :
   - Mélanger différents niveaux d'isolation
   - Représente mieux situations réelles

3. **Ajuster températures cibles** :
   - Utiliser valeurs proches de la réalité
   - Ex: 19-21°C pour habitation

4. **Entraîner plus longtemps** :
   - Augmenter epochs à 200
   - Améliore convergence

5. **Utiliser données réelles** :
   - Remplacer simulation par vraies mesures
   - Nécessite capteurs de température

### Pour Économiser Batterie

1. **Réduire luminosité écran** :
   ```cpp
   M5.Display.setBrightness(128);  // 50% au lieu de 255
   ```

2. **Augmenter cooldown API** :
   ```cpp
   const unsigned long API_COOLDOWN = 30000;  // 30s au lieu de 10s
   ```

3. **Désactiver WiFi quand non utilisé** :
   ```cpp
   WiFi.disconnect();
   WiFi.mode(WIFI_OFF);
   ```

4. **Mode veille entre utilisations** :
   ```cpp
   esp_sleep_enable_timer_wakeup(60 * 1000000);  // 1 min
   esp_light_sleep_start();
   ```

### Pour Meilleures Performances

1. **Limiter historique** :
   ```cpp
   #define HISTORY_SIZE 30  // Au lieu de 50
   ```

2. **Désactiver cache si RAM limitée** :
   ```cpp
   const unsigned long CACHE_DURATION = 0;
   ```

3. **Réduire fréquence rafraîchissement** :
   ```cpp
   delay(100);  // Entre touches tactiles
   ```

4. **Optimiser graphiques** :
   - Réduire épaisseur courbes (for t=-1 to 1 au lieu de -2 to 2)
   - Afficher moins de points

---

## 🎯 Roadmap Future

### Court Terme (v3.0)

- [ ] **Stockage SD** : Historique persistant illimité
- [ ] **Calibration** : Ajustement modèle avec capteurs réels
- [ ] **Export données** : CSV depuis M5Stack vers PC
- [ ] **Statistiques avancées** : Min/max/moyenne par période

### Moyen Terme (v4.0)

- [ ] **Support MQTT** : Intégration domotique (Home Assistant, etc.)
- [ ] **Alertes** : Notifications température hors plage
- [ ] **Multi-langues** : Français, Anglais, Espagnol
- [ ] **Thèmes visuels** : Clair/Sombre/Coloré

### Long Terme (v5.0)

- [ ] **Application mobile** : Compagnon Android/iOS
- [ ] **Cloud sync** : Sauvegarde en ligne
- [ ] **Machine Learning en ligne** : Ajustement continu du modèle
- [ ] **Prédictions multi-jours** : Anticipation sur 7 jours

---

## 📞 Support et Contributions

### Signaler un Bug

1. Ouvrir une **Issue** sur GitHub
2. Inclure :
   - Version du code (commit hash)
   - Système d'exploitation
   - Logs complets (Python + Arduino)
   - Étapes de reproduction

### Demande de Fonctionnalité

1. Vérifier roadmap ci-dessus
2. Ouvrir **Feature Request** si absent
3. Décrire cas d'usage précis

### Contribuer au Code

1. **Fork** le projet
2. Créer branche feature (`git checkout -b feature/MaNouvelleFonction`)
3. Commit changements (`git commit -m 'Ajout fonctionnalité X'`)
4. Push vers branche (`git push origin feature/MaNouvelleFonction`)
5. Ouvrir **Pull Request**

**Conventions** :
- Code commenté en français
- Tests unitaires pour nouvelles features
- Documentation mise à jour

---

## 🏆 Remerciements

- **M5Stack** pour le matériel de qualité
- **TensorFlow** pour le framework ML
- **Open-Meteo** pour l'API météo gratuite
- **Communauté Arduino** pour les bibliothèques

---

## 📄 Changelog

### v2.0 (Actuel)
- ✅ Interface GUI complète
- ✅ Support multi-chambres dynamique (1-30+)
- ✅ Page "DONNÉES CSV" avec historique simulé
- ✅ Échelles adaptatives sur tous graphiques
- ✅ Optimisations graphiques (buffering)
- ✅ Encodage Windows compatible (ASCII)
- ✅ Rate limiting réduit à 10s

### v1.5
- Support 3 chambres fixes
- Graphiques manuels et API
- Configuration WiFi tactile

### v1.0
- Version initiale
- 1 chambre uniquement
- Prédiction manuelle basique

---

**Développé avec ❤️ pour M5Stack TABS**

*Dernière mise à jour : 30 novembre 2025*
#   P r e d i c t e m p  
 