RoomPredictor – Prédiction de Température Multi-Chambres
1. Description

RoomPredictor est un système complet permettant de prédire la température intérieure de plusieurs chambres en utilisant un réseau de neurones entraîné sous Python et déployé sur un M5Stack TABS (ESP32).
Le projet inclut :

un générateur de données synthétiques,

un script d'entraînement TensorFlow,

l’export des poids vers un microcontrôleur,

une interface tactile embarquée permettant d'afficher prédictions et historiques.

2. Fonctionnement du Projet

Le pipeline complet est composé de trois étapes principales :

2.1 Génération de Données

Via data_generator_gui.py :

création de données simulées par chambre,

encodage saisonnier et journalier (sin/cos),

export en CSV.

2.2 Entraînement du Modèle

Via train_model_with_date.py :

normalisation (StandardScaler),

réseau 5 → 32 → 32 → N sorties,

métriques : MSE (perte), MAE (évaluation),

export automatique des poids en C++ (neural_weights.h).

2.3 Déploiement sur M5Stack

Dans RoomPredictor.ino :

chargement des poids en PROGMEM,

inférence temps réel (< 2 ms),

affichage graphique des prédictions et données historiques,

intégration API Open-Meteo.

3. Installation
3.1 Prérequis Python

Python 3.10+

pip

Installer les dépendances :

pip install -r requirements.txt

3.2 Prérequis Arduino / M5Stack

Arduino IDE 2.x

Carte : M5Stack TABS

Partition : Huge APP (3 MB)

Copier dans le dossier RoomPredictor/ :

neural_weights.h

csv_data.h

Puis téléverser RoomPredictor.ino.

4. Utilisation
Étape 1 — Générer les données
python data_generator_gui.py

Étape 2 — Entraîner le modèle
python train_model_with_date.py


Sorties :

temp_model_with_date.h5

neural_weights.h

Étape 3 — Exporter les données CSV vers Arduino
python export_csv_to_arduino_v2.py


Sortie :

csv_data.h

Étape 4 — Téléverser sur M5Stack

Ouvrir RoomPredictor.ino dans Arduino IDE et téléverser.

5. Structure du Projet (réduite)
/RoomPredictor
    RoomPredictor.ino
    neural_weights.h
    csv_data.h

/data
    Room1_data.csv
    Room2_data.csv
    ...

data_generator_gui.py
train_model_with_date.py
export_csv_to_arduino_v2.py
requirements.txt

6. Informations Essentielles

Nombre de chambres géré automatiquement (1 à 100+).

Inférence très rapide : environ 1.8 ms sur ESP32.

Modèle optimisé : ~1400 paramètres seulement.

Encodages cycliques pour saison/heure.

Données historiques stockées en PROGMEM.

Aucun fichier à modifier manuellement ; tout est généré automatiquement.

7. Problèmes Courants

Valeurs incohérentes → Vérifier que W0 a bien shape [5][32].

Freeze M5Stack → Partition incorrecte (utiliser Huge APP).

Données décalées → Regénérer les CSV.

8. Licence

Projet personnel à but pédagogique. Toute réutilisation doit mentionner l'auteur.
