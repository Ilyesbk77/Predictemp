# -*- coding: utf-8 -*-
"""
Training amélioré avec features temporelles enrichies
Architecture: Input(5) -> Dense(32) -> Dense(32) -> Output(N)
Features: température_ext, humidité, saison_sin, saison_cos, heure_jour_sin
"""

import pandas as pd
import numpy as np
from datetime import datetime
from tensorflow.keras import Sequential
from tensorflow.keras.layers import Dense, Input
from tensorflow.keras.optimizers import Adam
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import tensorflow as tf
import warnings
warnings.filterwarnings('ignore')

# ============================================================================
# CONFIGURATION
# ============================================================================

def get_csv_files():
    """Détecte automatiquement tous les fichiers RoomN_data.csv dans data/"""
    import os
    import glob
    
    csv_files = {}
    pattern = 'data/Room*_data.csv'
    
    for filepath in sorted(glob.glob(pattern)):
        filename = os.path.basename(filepath)
        # Extraire numéro de chambre (Room1, Room2, etc.)
        if filename.startswith('Room') and filename.endswith('_data.csv'):
            # Extraire le nom/numéro entre 'Room' et '_data.csv'
            room_id = filename[4:-9]  # 'Room1_data.csv' -> '1'
            csv_files[f'room{room_id}'] = filepath
    
    return csv_files

CSV_FILES = get_csv_files()

# Mapping flexible des colonnes - définir les noms possibles pour chaque type de colonne
COLUMN_MAPPINGS = {
    'timestamp': ['Timestamp', 'timestamp', 'Date', 'date', 'DateTime', 'datetime', 'Time', 'time'],
    'temperature': ['Temperature_Celsius(°C)', 'Temperature', 'temperature', 'Temp', 'temp', 
                    'Temperature(°C)', 'Temperature (°C)', 'Température', 'température'],
    'humidity': ['Relative_Humidity(%)', 'Humidity', 'humidity', 'RH', 'rh', 'Humidité', 'humidité',
                 'Relative Humidity', 'relative_humidity']
}

MODEL_FILE = 'rooms_model_with_date.h5'
WEIGHTS_FILE = 'rooms_model_with_date.weights.h5'

# ============================================================================
# CHARGEMENT ET PRÉPARATION DONNÉES
# ============================================================================

def day_of_year(date):
    """Calcule le jour de l'année (1-365)"""
    return date.timetuple().tm_yday

def encode_date_cyclical(date):
    """Encode la date de manière cyclique (sin/cos pour continuité)"""
    doy = day_of_year(date)
    angle = (2 * np.pi * doy) / 365.0
    return np.sin(angle), np.cos(angle)

def encode_season(date):
    """Encode la saison de manière cyclique"""
    # Utilise le jour de l'année pour encoder la saison (cycle de 365 jours)
    doy = day_of_year(date)
    angle = (2 * np.pi * doy) / 365.0
    return np.sin(angle), np.cos(angle)

def encode_time_of_day(date):
    """Encode l'heure de la journée de manière cyclique (0-24h)"""
    hour = date.hour + date.minute / 60.0
    angle = (2 * np.pi * hour) / 24.0
    return np.sin(angle)

def detect_column(df, column_type):
    """
    Détecte automatiquement quelle colonne correspond au type demandé
    
    Args:
        df: DataFrame pandas
        column_type: 'timestamp' ou 'temperature'
    
    Returns:
        Nom de la colonne détectée ou None
    """
    possible_names = COLUMN_MAPPINGS.get(column_type, [])
    
    # Chercher correspondance exacte (case-insensitive)
    for col in df.columns:
        if col in possible_names:
            return col
    
    # Chercher correspondance partielle
    for col in df.columns:
        col_lower = col.lower()
        for possible in possible_names:
            if possible.lower() in col_lower:
                return col
    
    return None

def load_room_data():
    """Charge et fusionne les données des rooms avec détection automatique des colonnes"""
    print("="*80)
    print("CHARGEMENT DES DONNÉES AVEC FEATURES TEMPORELLES")
    print("="*80)
    
    # Détecter les fichiers CSV disponibles
    csv_files = get_csv_files()
    
    if not csv_files:
        print("\n[WARN]  AUCUN FICHIER CSV TROUVÉ!")
        print("   Assurez-vous d'avoir des fichiers Room1_data.csv, Room2_data.csv, etc. dans data/")
        return pd.DataFrame()
    
    print(f"\n[FILES] Fichiers détectés: {len(csv_files)} chambres")
    for room_name, filepath in csv_files.items():
        print(f"   - {room_name}: {filepath}")
    print()
    
    all_data = []
    
    for room_name, csv_file in csv_files.items():
        try:
            df = pd.read_csv(csv_file)
            print(f"\n[OK] {csv_file}: {len(df)} lignes")
            print(f"  Colonnes disponibles: {list(df.columns)}")
            
            # Détection automatique des colonnes
            timestamp_col = detect_column(df, 'timestamp')
            temperature_col = detect_column(df, 'temperature')
            humidity_col = detect_column(df, 'humidity')
            
            if timestamp_col is None:
                print(f"  [WARN] Colonne timestamp non trouvée. Colonnes disponibles: {list(df.columns)}")
                print(f"     Essayez d'ajouter le nom de votre colonne dans COLUMN_MAPPINGS['timestamp']")
                continue
            
            if temperature_col is None:
                print(f"  [WARN] Colonne température non trouvée. Colonnes disponibles: {list(df.columns)}")
                print(f"     Essayez d'ajouter le nom de votre colonne dans COLUMN_MAPPINGS['temperature']")
                continue
            
            print(f"  -> Timestamp: '{timestamp_col}'")
            print(f"  -> Température: '{temperature_col}'")
            
            # Préparer colonnes à extraire
            cols_to_extract = [timestamp_col, temperature_col]
            rename_map = {
                timestamp_col: 'Timestamp',
                temperature_col: 'temperature'
            }
            
            # Ajouter humidité si disponible
            if humidity_col:
                print(f"  -> Humidité: '{humidity_col}'")
                cols_to_extract.append(humidity_col)
                rename_map[humidity_col] = 'humidity'
            else:
                print(f"  -> Humidité: Non disponible (sera simulée)")
            
            df_clean = df[cols_to_extract].copy()
            df_clean.rename(columns=rename_map, inplace=True)
            
            # Ajouter humidité par défaut si absente
            if 'humidity' not in df_clean.columns:
                df_clean['humidity'] = 50.0  # Valeur par défaut 50%
            
            df_clean['room_name'] = room_name
            
            all_data.append(df_clean)
            
        except Exception as e:
            print(f"[X] Erreur {csv_file}: {e}")
    
    if not all_data:
        raise ValueError("Aucune donnée chargée")
    
    merged = pd.concat(all_data, ignore_index=True)
    merged['Timestamp'] = pd.to_datetime(merged['Timestamp'])
    merged.sort_values('Timestamp', inplace=True)
    merged.reset_index(drop=True, inplace=True)
    
    print(f"\n[OK] Total: {len(merged)} entrées")
    print(f"  Période: {merged['Timestamp'].min()} -> {merged['Timestamp'].max()}\n")
    
    return merged

def prepare_features_with_date(df):
    """
    Prépare features enrichies:
    - température_ext (simulée)
    - humidité moyenne
    - saison (sin, cos)
    - heure du jour (sin)
    Targets: toutes les rooms disponibles
    """
    print("="*80)
    print("PRÉPARATION FEATURES ENRICHIES (5 ENTRÉES)")
    print("="*80)
    
    # Simuler température extérieure (modèle simplifié basé sur jour de l'année)
    # En production: utiliser vraies données météo Open-Meteo
    
    # Grouper par timestamp pour avoir tous les rooms ensemble
    grouped = df.groupby('Timestamp')
    
    # Détecter les chambres disponibles
    available_rooms = sorted(df['room_name'].unique())
    num_rooms = len(available_rooms)
    
    print(f"\n[ROOMS] Chambres disponibles: {num_rooms}")
    for room in available_rooms:
        print(f"   - {room}")
    print()
    
    X_list = []  # Features: [temp_ext, humidity, season_sin, season_cos, time_sin]
    y_list = []  # Targets: [room1, room2, ..., roomN]
    
    for timestamp, group in grouped:
        # Simuler température extérieure basée sur saison
        doy = day_of_year(timestamp)
        # Température moyenne annuelle + variation saisonnière
        temp_ext = 12.0 + 15.0 * np.sin((2 * np.pi * (doy - 80)) / 365.0)
        # Ajouter bruit aléatoire
        temp_ext += np.random.normal(0, 3.0)
        
        # Calculer humidité moyenne de toutes les rooms pour ce timestamp
        humidity_values = [row['humidity'] for _, row in group.iterrows() if 'humidity' in row]
        avg_humidity = np.mean(humidity_values) if humidity_values else 50.0  # Défaut 50%
        
        # Encodage cyclique de la saison (jour de l'année)
        season_sin, season_cos = encode_season(timestamp)
        
        # Encodage cyclique de l'heure de la journée
        time_sin = encode_time_of_day(timestamp)
        
        # Récupérer températures des rooms
        temps = {}
        for _, row in group.iterrows():
            temps[row['room_name']] = row['temperature']
        
        # Vérifier qu'on a toutes les chambres pour ce timestamp
        if all(room in temps for room in available_rooms):
            X_list.append([temp_ext, avg_humidity, season_sin, season_cos, time_sin])
            y_list.append([temps[room] for room in available_rooms])
    
    X = np.array(X_list)
    y = np.array(y_list)
    
    print(f"[OK] Features créées: {X.shape[0]} échantillons")
    print(f"  - Feature 1: Température extérieure (°C)")
    print(f"  - Feature 2: Humidité moyenne (%)")
    print(f"  - Feature 3: sin(saison/jour année)")
    print(f"  - Feature 4: cos(saison/jour année)")
    print(f"  - Feature 5: sin(heure du jour)")
    print(f"\n[OK] Targets: {num_rooms} chambres ({', '.join(available_rooms)})")
    
    if X.shape[0] > 0:
        print(f"\nStatistiques Features:")
        print(f"  Temp ext: min={X[:, 0].min():.1f}°C, max={X[:, 0].max():.1f}°C, mean={X[:, 0].mean():.1f}°C")
        print(f"  Humidité: min={X[:, 1].min():.1f}%, max={X[:, 1].max():.1f}%, mean={X[:, 1].mean():.1f}%")
        print(f"  sin(saison): min={X[:, 2].min():.3f}, max={X[:, 2].max():.3f}")
        print(f"  cos(saison): min={X[:, 3].min():.3f}, max={X[:, 3].max():.3f}")
        print(f"  sin(heure): min={X[:, 4].min():.3f}, max={X[:, 4].max():.3f}\n")
    else:
        print(f"\n[WARN]  AUCUNE DONNÉE: Les {num_rooms} CSV n'ont aucun timestamp commun!")
        print(f"   Vérifiez que les fichiers couvrent la même période.")
    
    return X, y, num_rooms

# ============================================================================
# MODÈLE RÉSEAU DE NEURONES
# ============================================================================

def create_model_with_date(num_outputs=3):
    """
    Architecture: Input(5) -> Dense(32, ReLU) -> Dense(32, ReLU) -> Output(N)
    
    Args:
        num_outputs: Nombre de chambres à prédire
    """
    print("="*80)
    print("CRÉATION MODÈLE AVEC FEATURES ENRICHIES (5 ENTRÉES)")
    print("="*80)
    
    model = Sequential([
        Input(shape=(5,)),  # 5 features: temp_ext, humidity, season_sin, season_cos, time_sin
        Dense(32, activation='relu', name='hidden1'),
        Dense(32, activation='relu', name='hidden2'),
        Dense(num_outputs, activation='linear', name='output')  # N chambres
    ])
    
    model.compile(
        optimizer=Adam(learning_rate=0.001),
        loss='mse',
        metrics=['mae']
    )
    
    print(model.summary())
    print(f"\n[OK] Architecture: 5 inputs -> 32 -> 32 -> {num_outputs} outputs")
    print(f"[OK] Total paramètres: {model.count_params()}")
    print(f"[OK] Optimiseur: Adam (lr=0.001)")
    print(f"[OK] Loss: MSE, Metric: MAE\n")
    
    return model

# ============================================================================
# ENTRAÎNEMENT
# ============================================================================

def train_model():
    """Pipeline complet: chargement, préparation, entraînement"""
    
    # 1. Charger données
    df = load_room_data()
    
    if df.empty:
        print("\n[ERROR] ERREUR: Aucune donnée disponible!")
        return None, None
    
    # 2. Préparer features avec date
    X, y, num_rooms = prepare_features_with_date(df)
    
    if X.shape[0] == 0:
        print("\n[ERROR] ERREUR: Aucun échantillon généré!")
        return None, None
    
    # 3. Split train/test
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    
    print(f"Train set: {X_train.shape[0]} échantillons")
    print(f"Test set: {X_test.shape[0]} échantillons\n")
    
    # 4. Créer modèle
    model = create_model_with_date(num_outputs=num_rooms)
    
    # 5. Entraîner
    print("="*80)
    print("ENTRAÎNEMENT")
    print("="*80)
    
    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=100,
        batch_size=16,
        verbose=1
    )
    
    # 6. Évaluation
    print("\n" + "="*80)
    print("ÉVALUATION SUR TEST SET")
    print("="*80)
    
    test_loss, test_mae = model.evaluate(X_test, y_test, verbose=0)
    print(f"[OK] Test Loss (MSE): {test_loss:.4f}")
    print(f"[OK] Test MAE: {test_mae:.4f}°C")
    
    # Prédictions exemples
    print("\nExemples de prédictions:")
    for i in range(min(5, len(X_test))):
        pred = model.predict(X_test[i:i+1], verbose=0)[0]
        true = y_test[i]
        print(f"\n  Exemple {i+1}:")
        print(f"    Input: temp_ext={X_test[i][0]:.1f}°C, humidity={X_test[i][1]:.1f}%, season_sin={X_test[i][2]:.3f}, season_cos={X_test[i][3]:.3f}, time_sin={X_test[i][4]:.3f}")
        print(f"    Vrai:  R1={true[0]:.1f}°C, R2={true[1]:.1f}°C, R3={true[2]:.1f}°C")
        print(f"    Préd:  R1={pred[0]:.1f}°C, R2={pred[1]:.1f}°C, R3={pred[2]:.1f}°C")
        print(f"    Erreur: R1={abs(pred[0]-true[0]):.1f}°C, R2={abs(pred[1]-true[1]):.1f}°C, R3={abs(pred[2]-true[2]):.1f}°C")
    
    # 7. Sauvegarder
    print("\n" + "="*80)
    print("SAUVEGARDE MODÈLE")
    print("="*80)
    
    model.save(MODEL_FILE)
    model.save_weights(WEIGHTS_FILE)
    print(f"[OK] Modèle sauvegardé: {MODEL_FILE}")
    print(f"[OK] Poids sauvegardés: {WEIGHTS_FILE}")
    
    return model, history

# ============================================================================
# EXPORT POUR ESP32
# ============================================================================

def export_weights_for_esp32(model, num_rooms=3):
    """Exporte les poids au format C++ pour ESP32"""
    print("\n" + "="*80)
    print("EXPORT POIDS POUR ESP32")
    print("="*80)
    
    # Récupérer poids de chaque couche
    weights = model.get_weights()
    
    # W0: (3, 32) - Input -> Hidden1
    # BIAS0: (32,)
    # W1: (32, 32) - Hidden1 -> Hidden2
    # BIAS1: (32,)
    # W2: (32, N) - Hidden2 -> Output
    # BIAS2: (N,)
    
    W0, BIAS0, W1, BIAS1, W2, BIAS2 = weights
    
    print(f"[OK] W0 shape: {W0.shape} (Input -> Hidden1)")
    print(f"[OK] BIAS0 shape: {BIAS0.shape}")
    print(f"[OK] W1 shape: {W1.shape} (Hidden1 -> Hidden2)")
    print(f"[OK] BIAS1 shape: {BIAS1.shape}")
    print(f"[OK] W2 shape: {W2.shape} (Hidden2 -> Output)")
    print(f"[OK] BIAS2 shape: {BIAS2.shape}")
    
    # Générer fichier C++
    output_file = 'neural_weights.h'
    with open(output_file, 'w') as f:
        f.write("// Auto-generated neural network weights\n")
        f.write(f"// Architecture: Input(5) -> Dense(32) -> Dense(32) -> Output({num_rooms})\n")
        f.write(f"// Features: temp_ext, humidity, season_sin, season_cos, time_sin\n")
        f.write(f"// Total parameters: {model.count_params()}\n")
        f.write(f"// Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        f.write("#ifndef NEURAL_WEIGHTS_H\n")
        f.write("#define NEURAL_WEIGHTS_H\n\n")
        
        f.write(f"// Configuration\n")
        f.write(f"#define NUM_ROOMS {num_rooms}\n\n")
        
        # W0: (5, 32) - 5 input features
        f.write(f"// Layer 0: Input(5) -> Dense(32)\n")
        f.write(f"const float W0[5][32] = {{\n")
        for i in range(5):
            f.write("  {")
            f.write(", ".join([f"{w:.6f}f" for w in W0[i]]))
            f.write(f"}},{'  // Input feature ' + str(i)}\n")
        f.write("};\n\n")
        
        # BIAS0
        f.write(f"const float BIAS0[32] = {{\n  ")
        f.write(", ".join([f"{b:.6f}f" for b in BIAS0]))
        f.write("\n};\n\n")
        
        # W1: (32, 32)
        f.write(f"// Layer 1: Dense(32) -> Dense(32)\n")
        f.write(f"const float W1[32][32] = {{\n")
        for i in range(32):
            f.write("  {")
            f.write(", ".join([f"{w:.6f}f" for w in W1[i]]))
            f.write("},\n")
        f.write("};\n\n")
        
        # BIAS1
        f.write(f"const float BIAS1[32] = {{\n  ")
        f.write(", ".join([f"{b:.6f}f" for b in BIAS1]))
        f.write("\n};\n\n")
        
        # W2: (32, N)
        f.write(f"// Layer 2: Dense(32) -> Output({num_rooms})\n")
        f.write(f"const float W2[32][{num_rooms}] = {{\n")
        for i in range(32):
            f.write("  {")
            f.write(", ".join([f"{w:.6f}f" for w in W2[i]]))
            f.write("},\n")
        f.write("};\n\n")
        
        # BIAS2
        f.write(f"const float BIAS2[{num_rooms}] = {{\n  ")
        f.write(", ".join([f"{b:.6f}f" for b in BIAS2]))
        f.write("\n};\n\n")
        
        f.write("#endif // NEURAL_WEIGHTS_H\n")
    
    print(f"\n[OK] Fichier généré: {output_file}")
    
    # Copier vers dossier M5Stack si existant
    import os
    import shutil
    m5stack_path = os.path.join('M5Stack_Temperature_Prediction', 'RoomPredictor', 'neural_weights.h')
    if os.path.exists('M5Stack_Temperature_Prediction'):
        try:
            shutil.copy(output_file, m5stack_path)
            print(f"[OK] Copié vers: {m5stack_path}")
        except Exception as e:
            print(f"[WARN] Impossible de copier vers M5Stack: {e}")
    
    print(f"[OK] Prêt pour upload sur M5Stack TABS\n")

# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    print("\n" + "="*80)
    print("ENTRAÎNEMENT MODÈLE AVEC FEATURES TEMPORELLES")
    print("="*80 + "\n")
    
    # Entraîner
    model, history = train_model()
    
    if model is None:
        print("\n[ERROR] Entraînement échoué!")
        exit(1)
    
    # Récupérer nombre de chambres
    num_rooms = model.output_shape[-1]
    
    # Exporter pour ESP32
    export_weights_for_esp32(model, num_rooms)
    
    print("\n" + "="*80)
    print("[OK] TERMINÉ AVEC SUCCÈS")
    print("="*80)
    print(f"\n[ROOMS] Modèle entraîné pour {num_rooms} chambres")
    print("\nProchaines étapes:")
    print("1. Vérifier neural_weights.h dans M5Stack_Temperature_Prediction/RoomPredictor/")
    print("2. Compiler et uploader sur M5Stack TABS")
    print("3. Tester prédictions avec différentes dates")
    print("\n")




