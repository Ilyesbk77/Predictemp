# 📘 Guide d'Utilisation - Prédiction de Température Multi-Pièces

## Vue d'ensemble

Ce guide explique comment utiliser l'application de génération de données, entraîner l'IA et déployer le firmware sur votre M5Stack.

---

## 🚀 Étape 1 : Lancer l'Application GUI

### Commande de lancement

```powershell
.venv\Scripts\python.exe data_generator_gui.py
```

Ou si vous êtes déjà dans l'environnement virtuel activé :

```powershell
python data_generator_gui.py
```

### Interface graphique

L'application s'ouvrira avec une interface Tkinter comprenant :
- Configuration du nombre de pièces
- Paramètres de génération de données
- Boutons d'import/export
- Visualisation graphique
- Entraînement du modèle

---

## 🏠 Étape 2 : Configurer le Nombre de Pièces

1. **Dans la section "Configuration"** en haut de l'interface
2. **Sélectionnez le nombre de pièces** (1 à X rooms)
   - Exemple : 6 pièces pour une maison avec salon, chambres, cuisine, etc.
3. Ce nombre déterminera combien de fichiers CSV seront générés/importés

---

## 📊 Étape 3 : Obtenir les Données

Vous avez **deux options** :

### Option A : Générer Automatiquement (Recommandé pour Tests)

1. **Configurez les paramètres de génération** :
   - **Période (jours)** : Nombre de jours à générer (ex: 365 pour 1 an)
   - **Intervalle (min)** : Fréquence d'échantillonnage (5, 10, 15, 30, ou 60 minutes)
   - **Date de début** : Année / Mois / Jour (ex: 2024 / 01 / 01)

2. **Pour chaque pièce, sélectionnez un profil d'isolation** :
   - **Excellente isolation** : Maison passive (19-22°C, faible variation)
   - **Bonne isolation** : RT 2012 (18-23°C)
   - **Isolation moyenne** : Années 1990 (16-24°C)
   - **Isolation faible** : Avant 1975 (14-26°C)
   - **Très mauvaise isolation** : Non rénové (12-28°C)
   
   *Note : L'humidité est automatiquement générée (30-70%) avec corrélation saisonnière*

3. **Cliquez sur "Générer Données Automatiquement"**
   - Les fichiers `Room1_data.csv`, `Room2_data.csv`, etc. seront créés dans `data/`
   - Chaque fichier contiendra : `timestamp`, `temp_ext`, `temp_int`, `humidity`

### Option B : Importer vos Propres Données

1. **Préparez vos fichiers CSV** avec le format suivant :
   ```csv
   timestamp,temp_ext,temp_int,humidity
   2024-01-01 00:00:00,5.2,19.5,45.0
   2024-01-01 00:30:00,5.0,19.4,46.0
   ...
   ```

2. **Nommez vos fichiers** : `Room1_data.csv`, `Room2_data.csv`, etc.

3. **Placez-les dans le dossier** `data/`

4. **Cliquez sur "Importer Données Existantes"** dans la GUI

---

## 📈 Étape 4 : Visualiser les Graphiques (Optionnel)

1. **Après génération/import des données**, cliquez sur **"Visualiser Graphiques"**

2. **Une fenêtre Matplotlib s'ouvrira** avec :
   - Courbes de température pour chaque pièce
   - Température extérieure
   - Humidité
   - Statistiques (min, max, moyenne, écart-type)

3. **Vérifiez visuellement** :
   - Cohérence des données
   - Variations saisonnières
   - Corrélations température intérieure/extérieure

4. **Fermez la fenêtre** pour revenir à la GUI

---

## 🧠 Étape 5 : Entraîner l'Intelligence Artificielle

1. **Cliquez sur "Entraîner Modèle"**

2. **L'entraînement démarre** avec :
   - Barre de progression en temps réel
   - Affichage des époques (ex: Epoch 50/100)
   - Calcul de la MAE (Mean Absolute Error)

3. **Durée estimée** :
   - 6 pièces, 365 jours : ~2-3 minutes
   - 10 pièces, 365 jours : ~4-5 minutes

4. **Résultats affichés** :
   - **Test MAE** : Précision du modèle (ex: 1.79°C)
   - Plus la MAE est basse, meilleur est le modèle

5. **Fichiers générés automatiquement** :
   - `neural_weights.h` : Poids du réseau de neurones (1,446 paramètres)
   - `csv_data.h` : Données historiques pour l'affichage
   - Ces fichiers sont **automatiquement copiés** dans `M5Stack_Temperature_Prediction/RoomPredictor/`

### Architecture du Modèle

Le réseau de neurones utilise **5 caractéristiques d'entrée** :
- **temp_ext** : Température extérieure
- **humidity** : Humidité
- **season_sin** : Encodage cyclique de la saison (sinus)
- **season_cos** : Encodage cyclique de la saison (cosinus)
- **time_sin** : Encodage cyclique de l'heure (sinus)

Structure :
```
Input(5) → Dense(32, ReLU) → Dense(32, ReLU) → Output(NUM_ROOMS)
```

---

## 📤 Étape 6 : Uploader le Firmware sur le M5Stack

### Prérequis

- **Arduino IDE** installé (version 2.x recommandée)
- **Bibliothèques M5Stack** installées
- **M5Stack TABS** connecté en USB

### Procédure

1. **Ouvrez Arduino IDE**

2. **Ouvrez le fichier** :
   ```
   M5Stack_Temperature_Prediction/RoomPredictor/RoomPredictor.ino
   ```

3. **Vérifiez les fichiers générés** sont présents dans le même dossier :
   - `neural_weights.h` ✅
   - `csv_data.h` ✅

4. **Configurez Arduino IDE** :
   - **Carte** : M5Stack-TABS
   - **Port** : Sélectionnez le port COM de votre M5Stack
   - **Upload Speed** : 921600

5. **Compilez le projet** :
   - Cliquez sur ✓ (Vérifier)
   - Vérifiez qu'il n'y a **aucune erreur de compilation**
   - Taille estimée : ~500 KB

6. **Uploadez sur le M5Stack** :
   - Cliquez sur → (Téléverser)
   - Attendez la fin de l'upload (~30 secondes)
   - Le M5Stack redémarre automatiquement

---

## 🎯 Étape 7 : Tester sur le M5Stack

### Navigation dans l'interface

Le firmware dispose de **10 pages** :

1. **Page 1-3** : Informations système
2. **Page 4** : Graph manuel (prédictions instantanées)
3. **Page 5** : **PREDICTION TEMP** (prédictions API)
4. **Page 6** : **DONNÉES CSV** (historique)
5. **Page 7-10** : Pages supplémentaires

### Test de la Page 5 (PREDICTION TEMP)

1. **Accédez à la page 5** en touchant l'écran
2. **Vérifiez** :
   - Toutes les pièces (Room 1-6) affichées avec des couleurs uniques
   - Courbes de prédiction cohérentes (15-25°C typiquement)
   - Légende compacte en haut à gauche
   - Graduations verticales à droite de la légende

### Test de la Page 6 (DONNÉES CSV)

1. **Accédez à la page 6**
2. **Testez le menu déroulant** :
   - Touchez la zone du dropdown en haut
   - Sélectionnez **"TOUTES"** : Affiche toutes les pièces
   - Sélectionnez **"ROOM1"**, **"ROOM2"**, etc. : Affiche une pièce spécifique
3. **Vérifiez** :
   - Toutes vos pièces sont listées (jusqu'à 100+)
   - Données historiques affichées correctement
   - Pas de chevauchement entre légende et graphique

---

## 🔧 Résolution de Problèmes

### La GUI ne se lance pas

```powershell
# Vérifiez que l'environnement virtuel est activé
.venv\Scripts\Activate.ps1

# Vérifiez les dépendances
pip install -r requirements.txt
```

### Erreur "W0 shape incorrect"

✅ **Résolu** : Le script `train_model_with_date.py` génère maintenant correctement `W0[5][32]` pour les 5 caractéristiques d'entrée.

### Températures aberrantes (-32 milliards °C)

✅ **Résolu** : Bug de corruption mémoire corrigé. Le fichier `neural_weights.h` a désormais la bonne structure.

### Dropdown limité à 3 pièces

✅ **Résolu** : Le dropdown est maintenant dynamique et supporte jusqu'à 100+ pièces.

### Légende hors écran

✅ **Résolu** : Légende repositionnée en haut à gauche (80px × hauteur dynamique).

---

## 📁 Structure des Fichiers Générés

Après un entraînement complet, vous aurez :

```
Predictemp/
├── data/
│   ├── Room1_data.csv          # Données pièce 1
│   ├── Room2_data.csv          # Données pièce 2
│   └── ...
├── models/
│   └── temp_model_with_date.h5 # Modèle TensorFlow complet
├── neural_weights.h            # Poids exportés (copie locale)
├── csv_data.h                  # Données CSV (copie locale)
└── M5Stack_Temperature_Prediction/
    └── RoomPredictor/
        ├── RoomPredictor.ino   # Firmware principal (3230 lignes)
        ├── neural_weights.h    # Poids pour ESP32 ✅
        └── csv_data.h          # Données pour ESP32 ✅
```

---

## 🎓 Conseils d'Utilisation

### Pour de Meilleures Prédictions

1. **Utilisez au moins 6 mois de données** (365 jours recommandé)
2. **Intervalle de 30 minutes** pour un bon équilibre précision/taille
3. **Données réelles** > données générées (si disponibles)
4. **Humidité cohérente** : 30-70% typiquement

### Scalabilité

- **6 pièces** : 131 KB de données CSV ✅
- **100 pièces** : ~1.9 MB (fonctionne sur ESP32)
- **1000 pièces** : ~19 MB (nécessite carte SD)

Le système est conçu pour supporter **un nombre illimité de pièces** grâce à l'architecture en tableaux linéaires avec indexation.

---

## 🆘 Support

En cas de problème :

1. **Vérifiez les logs** dans le terminal pendant l'entraînement
2. **Consultez le README.md** pour la documentation complète
3. **Vérifiez que `neural_weights.h` contient** `W0[5][32]` (pas `W0[3][32]`)
4. **Assurez-vous que** `csv_data.h` définit `CSV_NUM_ROOMS` égal à votre nombre de pièces

---

## ✅ Checklist Complète

- [ ] Lancer `data_generator_gui.py`
- [ ] Configurer le nombre de pièces
- [ ] Générer ou importer les données CSV
- [ ] Visualiser les graphiques (optionnel)
- [ ] Entraîner le modèle IA
- [ ] Vérifier MAE < 2.0°C
- [ ] Vérifier que `neural_weights.h` et `csv_data.h` sont copiés
- [ ] Ouvrir `RoomPredictor.ino` dans Arduino IDE
- [ ] Compiler sans erreur
- [ ] Uploader sur M5Stack
- [ ] Tester Page 5 (PREDICTION TEMP)
- [ ] Tester Page 6 (DONNÉES CSV) avec dropdown

---

**Temps total estimé** : 10-15 minutes de la génération à l'upload 🚀
