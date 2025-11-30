#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Interface Graphique pour la Gestion des Données de Température
Permet de générer des données fictives OU d'importer des données réelles
Support de N chambres (1 à 20+)
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
import os
import json
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

# Profils d'isolation
ISOLATION_PROFILES = {
    'excellent': {
        'name': 'Excellente isolation (maison passive)',
        'thermal_inertia': 0.95,
        'external_influence': 0.10,
        'heating_efficiency': 0.90,
        'temp_range': (19, 22),
        'daily_variation': 1.0,
        'seasonal_amplitude': 3.0,
        'color': '#2ECC71'
    },
    'good': {
        'name': 'Bonne isolation (RT 2012)',
        'thermal_inertia': 0.80,
        'external_influence': 0.25,
        'heating_efficiency': 0.75,
        'temp_range': (18, 23),
        'daily_variation': 2.0,
        'seasonal_amplitude': 5.0,
        'color': '#3498DB'
    },
    'medium': {
        'name': 'Isolation moyenne (années 1990)',
        'thermal_inertia': 0.60,
        'external_influence': 0.45,
        'heating_efficiency': 0.60,
        'temp_range': (16, 24),
        'daily_variation': 3.5,
        'seasonal_amplitude': 8.0,
        'color': '#F39C12'
    },
    'poor': {
        'name': 'Isolation faible (avant 1975)',
        'thermal_inertia': 0.35,
        'external_influence': 0.70,
        'heating_efficiency': 0.45,
        'temp_range': (14, 26),
        'daily_variation': 5.0,
        'seasonal_amplitude': 12.0,
        'color': '#E74C3C'
    },
    'very_poor': {
        'name': 'Très mauvaise isolation (non rénové)',
        'thermal_inertia': 0.20,
        'external_influence': 0.85,
        'heating_efficiency': 0.30,
        'temp_range': (12, 28),
        'daily_variation': 6.5,
        'seasonal_amplitude': 15.0,
        'color': '#8E44AD'
    }
}

class RoomConfig:
    """Configuration pour une chambre"""
    def __init__(self, room_id, name="", profile='good', use_real_data=False, csv_path=""):
        self.room_id = room_id
        self.name = name or f"Room {room_id}"
        self.profile = profile
        self.use_real_data = use_real_data
        self.csv_path = csv_path

class DataGeneratorGUI:
    """Interface graphique principale"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("🌡️ RoomPredictor - Générateur de Données")
        self.root.geometry("1200x800")
        
        # Rendre la fenêtre responsive
        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(0, weight=1)
        
        # Configuration
        self.rooms = []  # Liste de RoomConfig
        self.config_file = "data_generator_config.json"
        
        # Style
        self.setup_style()
        
        # Interface
        self.create_widgets()
        
        # Charger config précédente
        self.load_config()
        
    def setup_style(self):
        """Configure le style de l'interface"""
        style = ttk.Style()
        style.theme_use('clam')
        
        # Couleurs
        style.configure('Title.TLabel', font=('Arial', 14, 'bold'), foreground='#2C3E50')
        style.configure('Header.TLabel', font=('Arial', 11, 'bold'), foreground='#34495E')
        style.configure('Room.TFrame', relief='solid', borderwidth=1)
        
    def create_widgets(self):
        """Crée l'interface utilisateur"""
        
        # === BARRE DE TITRE ===
        title_frame = ttk.Frame(self.root, padding="10")
        title_frame.pack(fill='x')
        
        title = ttk.Label(title_frame, text="🌡️ Générateur de Données pour RoomPredictor", 
                         style='Title.TLabel')
        title.pack()
        
        subtitle = ttk.Label(title_frame, 
                           text="Générez des données fictives ou importez vos données réelles",
                           foreground='gray')
        subtitle.pack()
        
        # === FRAME PRINCIPAL AVEC SCROLL ===
        main_container = ttk.Frame(self.root)
        main_container.pack(fill='both', expand=True, padx=10, pady=5)
        
        # Canvas + Scrollbar
        canvas = tk.Canvas(main_container)
        scrollbar = ttk.Scrollbar(main_container, orient="vertical", command=canvas.yview)
        self.scrollable_frame = ttk.Frame(canvas)
        
        # Configurer le canvas pour qu'il se redimensionne
        def on_frame_configure(event):
            canvas.configure(scrollregion=canvas.bbox("all"))
        
        def on_canvas_configure(event):
            # Faire en sorte que le scrollable_frame remplisse la largeur du canvas
            canvas.itemconfig(canvas_window, width=event.width)
        
        self.scrollable_frame.bind("<Configure>", on_frame_configure)
        canvas.bind("<Configure>", on_canvas_configure)
        
        canvas_window = canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # === CONFIGURATION GLOBALE ===
        global_frame = ttk.LabelFrame(self.scrollable_frame, text="⚙️ Configuration Globale", padding="10")
        global_frame.pack(fill='x', padx=5, pady=5)
        
        # Nombre de chambres
        rooms_frame = ttk.Frame(global_frame)
        rooms_frame.pack(fill='x', pady=5)
        
        ttk.Label(rooms_frame, text="Nombre de chambres:", style='Header.TLabel').pack(side='left', padx=5)
        
        self.num_rooms_var = tk.IntVar(value=3)
        num_rooms_spinbox = ttk.Spinbox(rooms_frame, from_=1, to=30, textvariable=self.num_rooms_var, 
                                       width=10, command=self.update_rooms_display)
        num_rooms_spinbox.pack(side='left', padx=5)
        
        ttk.Button(rooms_frame, text="📝 Appliquer", command=self.update_rooms_display).pack(side='left', padx=5)
        
        # Paramètres de génération
        params_frame = ttk.Frame(global_frame)
        params_frame.pack(fill='x', pady=5)
        
        ttk.Label(params_frame, text="Période (jours):", style='Header.TLabel').pack(side='left', padx=5)
        self.days_var = tk.IntVar(value=365)
        ttk.Spinbox(params_frame, from_=7, to=730, textvariable=self.days_var, width=10).pack(side='left', padx=5)
        
        ttk.Label(params_frame, text="Intervalle (min):", style='Header.TLabel').pack(side='left', padx=15)
        self.interval_var = tk.IntVar(value=30)
        interval_combo = ttk.Combobox(params_frame, textvariable=self.interval_var, width=8, 
                                     values=[5, 10, 15, 30, 60])
        interval_combo.pack(side='left', padx=5)
        
        # Date de début
        date_frame = ttk.Frame(global_frame)
        date_frame.pack(fill='x', pady=5)
        
        ttk.Label(date_frame, text="Date de début:", style='Header.TLabel').pack(side='left', padx=5)
        
        now = datetime.now()
        self.start_year_var = tk.IntVar(value=now.year - 1)
        self.start_month_var = tk.IntVar(value=now.month)
        self.start_day_var = tk.IntVar(value=now.day)
        
        ttk.Spinbox(date_frame, from_=2020, to=2030, textvariable=self.start_year_var, width=8).pack(side='left', padx=2)
        ttk.Label(date_frame, text="/").pack(side='left')
        ttk.Spinbox(date_frame, from_=1, to=12, textvariable=self.start_month_var, width=5).pack(side='left', padx=2)
        ttk.Label(date_frame, text="/").pack(side='left')
        ttk.Spinbox(date_frame, from_=1, to=31, textvariable=self.start_day_var, width=5).pack(side='left', padx=2)
        
        # === LISTE DES CHAMBRES ===
        self.rooms_container = ttk.Frame(self.scrollable_frame)
        self.rooms_container.pack(fill='both', expand=True, padx=5, pady=5)
        
        # === ZONE DE LOG ===
        log_frame = ttk.LabelFrame(self.scrollable_frame, text="📋 Journal", padding="5")
        log_frame.pack(fill='both', expand=True, padx=5, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, height=8, wrap=tk.WORD, 
                                                 font=('Consolas', 9))
        self.log_text.pack(fill='both', expand=True)
        
        # === BOUTONS D'ACTION ===
        action_frame = ttk.Frame(self.root, padding="10")
        action_frame.pack(fill='x')
        
        ttk.Button(action_frame, text="💾 Sauvegarder Configuration", 
                  command=self.save_config).pack(side='left', padx=5)
        
        ttk.Button(action_frame, text="📂 Charger Configuration", 
                  command=self.load_config).pack(side='left', padx=5)
        
        ttk.Button(action_frame, text="📊 Visualiser Données CSV", 
                  command=self.visualize_csv_data).pack(side='left', padx=5)
        
        ttk.Button(action_frame, text="📤 Exporter vers Arduino", 
                  command=self.manual_export_to_arduino).pack(side='left', padx=5)
        
        ttk.Button(action_frame, text="🚀 GÉNÉRER LES DONNÉES", 
                  command=self.generate_all_data, 
                  style='Accent.TButton').pack(side='right', padx=5)
        
        ttk.Button(action_frame, text="🔄 Entraîner le Modèle", 
                  command=self.train_model).pack(side='right', padx=5)
        
        # Style bouton accent
        style = ttk.Style()
        style.configure('Accent.TButton', font=('Arial', 10, 'bold'))
        
        # Initialiser l'affichage
        self.update_rooms_display()
        
    def update_rooms_display(self):
        """Met à jour l'affichage des chambres selon le nombre choisi"""
        # Supprimer anciens widgets
        for widget in self.rooms_container.winfo_children():
            widget.destroy()
        
        num_rooms = self.num_rooms_var.get()
        
        # Ajuster la liste de configs
        while len(self.rooms) < num_rooms:
            self.rooms.append(RoomConfig(len(self.rooms) + 1))
        while len(self.rooms) > num_rooms:
            self.rooms.pop()
        
        # Créer les widgets pour chaque chambre
        for i, room in enumerate(self.rooms):
            self.create_room_widget(i, room)
    
    def create_room_widget(self, index, room):
        """Crée le widget pour une chambre"""
        frame = ttk.LabelFrame(self.rooms_container, text=f"🏠 Chambre {room.room_id}", 
                              padding="10", style='Room.TFrame')
        frame.pack(fill='x', pady=5)
        
        # Ligne 1: Nom
        name_frame = ttk.Frame(frame)
        name_frame.pack(fill='x', pady=2)
        
        ttk.Label(name_frame, text="Nom:", width=15).pack(side='left')
        name_entry = ttk.Entry(name_frame, width=30)
        name_entry.insert(0, room.name)
        name_entry.pack(side='left', padx=5)
        name_entry.bind('<KeyRelease>', lambda e, idx=index: self.update_room_name(idx, name_entry.get()))
        
        # Ligne 2: Source de données
        source_frame = ttk.Frame(frame)
        source_frame.pack(fill='x', pady=2)
        
        ttk.Label(source_frame, text="Source:", width=15).pack(side='left')
        
        use_real_var = tk.BooleanVar(value=room.use_real_data)
        
        real_radio = ttk.Radiobutton(source_frame, text="📁 Données réelles (CSV)", 
                                    variable=use_real_var, value=True,
                                    command=lambda idx=index, var=use_real_var: self.toggle_data_source(idx, var))
        real_radio.pack(side='left', padx=5)
        
        fake_radio = ttk.Radiobutton(source_frame, text="🎲 Données générées", 
                                    variable=use_real_var, value=False,
                                    command=lambda idx=index, var=use_real_var: self.toggle_data_source(idx, var))
        fake_radio.pack(side='left', padx=5)
        
        # Ligne 3: Options selon source
        options_frame = ttk.Frame(frame)
        options_frame.pack(fill='x', pady=2)
        
        ttk.Label(options_frame, text="", width=15).pack(side='left')  # Alignement
        
        if room.use_real_data:
            # Import CSV
            csv_label = ttk.Label(options_frame, text=room.csv_path or "Aucun fichier sélectionné", 
                                 foreground='gray')
            csv_label.pack(side='left', padx=5)
            
            ttk.Button(options_frame, text="📂 Parcourir...", 
                      command=lambda idx=index: self.browse_csv(idx)).pack(side='left', padx=5)
        else:
            # Profil d'isolation
            ttk.Label(options_frame, text="Profil d'isolation:", foreground='#34495E').pack(side='left', padx=5)
            
            profile_var = tk.StringVar(value=room.profile)
            profile_combo = ttk.Combobox(options_frame, textvariable=profile_var, width=30, state='readonly',
                                        values=list(ISOLATION_PROFILES.keys()))
            profile_combo.pack(side='left', padx=5)
            profile_combo.bind('<<ComboboxSelected>>', 
                              lambda e, idx=index, var=profile_var: self.update_room_profile(idx, var.get()))
            
            # Afficher description
            profile_desc = ISOLATION_PROFILES[room.profile]['name']
            ttk.Label(options_frame, text=f"({profile_desc})", foreground='gray').pack(side='left', padx=5)
        
        # Stocker la référence pour mise à jour dynamique
        room._widget_frame = frame
        room._use_real_var = use_real_var
    
    def update_room_name(self, index, name):
        """Met à jour le nom d'une chambre"""
        self.rooms[index].name = name
    
    def toggle_data_source(self, index, var):
        """Change la source de données (réel/généré)"""
        self.rooms[index].use_real_data = var.get()
        # Rafraîchir l'affichage
        self.update_rooms_display()
    
    def update_room_profile(self, index, profile):
        """Met à jour le profil d'isolation"""
        self.rooms[index].profile = profile
    
    def browse_csv(self, index):
        """Ouvre un dialogue pour sélectionner un CSV"""
        filename = filedialog.askopenfilename(
            title=f"Sélectionner CSV pour {self.rooms[index].name}",
            filetypes=[("Fichiers CSV", "*.csv"), ("Tous fichiers", "*.*")]
        )
        if filename:
            self.rooms[index].csv_path = filename
            self.log(f"✓ Fichier sélectionné pour {self.rooms[index].name}: {filename}")
            self.update_rooms_display()
    
    def log(self, message):
        """Affiche un message dans le journal"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.log_text.see(tk.END)
        self.root.update()
    
    def save_config(self):
        """Sauvegarde la configuration"""
        config = {
            'num_rooms': self.num_rooms_var.get(),
            'days': self.days_var.get(),
            'interval': self.interval_var.get(),
            'start_date': {
                'year': self.start_year_var.get(),
                'month': self.start_month_var.get(),
                'day': self.start_day_var.get()
            },
            'rooms': [
                {
                    'room_id': r.room_id,
                    'name': r.name,
                    'profile': r.profile,
                    'use_real_data': r.use_real_data,
                    'csv_path': r.csv_path
                }
                for r in self.rooms
            ]
        }
        
        with open(self.config_file, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        
        self.log(f"💾 Configuration sauvegardée: {self.config_file}")
        messagebox.showinfo("Succès", "Configuration sauvegardée avec succès!")
    
    def load_config(self):
        """Charge la configuration précédente"""
        if not os.path.exists(self.config_file):
            return
        
        try:
            with open(self.config_file, 'r', encoding='utf-8') as f:
                config = json.load(f)
            
            self.num_rooms_var.set(config.get('num_rooms', 3))
            self.days_var.set(config.get('days', 365))
            self.interval_var.set(config.get('interval', 30))
            
            start = config.get('start_date', {})
            self.start_year_var.set(start.get('year', datetime.now().year - 1))
            self.start_month_var.set(start.get('month', datetime.now().month))
            self.start_day_var.set(start.get('day', datetime.now().day))
            
            self.rooms = []
            for r_config in config.get('rooms', []):
                room = RoomConfig(
                    r_config['room_id'],
                    r_config.get('name', f"Room {r_config['room_id']}"),
                    r_config.get('profile', 'good'),
                    r_config.get('use_real_data', False),
                    r_config.get('csv_path', '')
                )
                self.rooms.append(room)
            
            self.update_rooms_display()
            self.log(f"📂 Configuration chargée: {self.config_file}")
            
        except Exception as e:
            self.log(f"⚠️  Erreur chargement config: {e}")
    
    def generate_all_data(self):
        """Génère toutes les données selon la configuration"""
        try:
            self.log("="*60)
            self.log("🚀 DÉBUT DE LA GÉNÉRATION")
            self.log("="*60)
            
            # Supprimer tous les fichiers CSV existants
            import glob
            existing_csvs = glob.glob("data/Room*_data.csv")
            if existing_csvs:
                for csv_file in existing_csvs:
                    os.remove(csv_file)
                self.log(f"🗑️  {len(existing_csvs)} fichier(s) CSV existant(s) supprimé(s)")
                self.log("")
            
            # Créer dossier data s'il n'existe pas
            os.makedirs("data", exist_ok=True)
            
            # Date de départ
            start_date = datetime(
                self.start_year_var.get(),
                self.start_month_var.get(),
                self.start_day_var.get()
            )
            
            num_days = self.days_var.get()
            interval = self.interval_var.get()
            
            self.log(f"📅 Période: {num_days} jours depuis {start_date.strftime('%Y-%m-%d')}")
            self.log(f"⏱️  Intervalle: {interval} minutes")
            self.log(f"🏠 Chambres: {len(self.rooms)}")
            self.log("")
            
            # Traiter chaque chambre
            for i, room in enumerate(self.rooms, 1):
                self.log(f"[{i}/{len(self.rooms)}] Traitement: {room.name}")
                
                if room.use_real_data:
                    # Copier fichier CSV réel
                    if not room.csv_path or not os.path.exists(room.csv_path):
                        self.log(f"  ⚠️  Fichier introuvable: {room.csv_path}")
                        continue
                    
                    df = pd.read_csv(room.csv_path)
                    output_file = f"data/Room{room.room_id}_data.csv"
                    df.to_csv(output_file, index=False)
                    self.log(f"  ✓ Copié: {len(df)} lignes → {output_file}")
                    
                else:
                    # Générer données fictives
                    profile = ISOLATION_PROFILES[room.profile]
                    self.log(f"  Profil: {profile['name']}")
                    
                    df = self.generate_synthetic_data(
                        room.profile,
                        start_date,
                        num_days,
                        interval
                    )
                    
                    output_file = f"data/Room{room.room_id}_data.csv"
                    df.to_csv(output_file, index=False)
                    self.log(f"  ✓ Généré: {len(df)} lignes → {output_file}")
                    self.log(f"    Temp: {df['Temperature_Celsius(°C)'].min():.1f}°C - {df['Temperature_Celsius(°C)'].max():.1f}°C (moy: {df['Temperature_Celsius(°C)'].mean():.1f}°C)")
                
                self.log("")
            
            self.log("="*60)
            self.log("✅ GÉNÉRATION TERMINÉE")
            self.log("="*60)
            
            messagebox.showinfo("Succès", 
                              f"{len(self.rooms)} fichiers CSV générés dans le dossier 'data/'!\n\n"
                              "Vous pouvez maintenant entraîner le modèle.")
            
        except Exception as e:
            self.log(f"❌ ERREUR: {e}")
            messagebox.showerror("Erreur", f"Erreur lors de la génération:\n{e}")
    
    def generate_synthetic_data(self, profile_key, start_date, num_days, interval_minutes):
        """Génère des données synthétiques pour une chambre"""
        from generate_test_data import generate_external_temperature, generate_room_temperature
        import time
        
        # Seed aléatoire basé sur timestamp pour avoir des données différentes à chaque génération
        random_seed = int(time.time() * 1000) % 100000
        np.random.seed(random_seed)
        
        profile = ISOLATION_PROFILES[profile_key]
        num_samples = int(num_days * 24 * 60 / interval_minutes)
        
        timestamps = []
        temperatures = []
        external_temps = []
        
        current_date = start_date
        # Température initiale avec variation aléatoire
        temp_avg = (profile['temp_range'][0] + profile['temp_range'][1]) / 2
        current_temp = temp_avg + np.random.uniform(-1, 1)
        
        for _ in range(num_samples):
            # Température extérieure
            ext_temp = generate_external_temperature(current_date)
            
            # Saison de chauffage
            month = current_date.month
            is_heating = month >= 10 or month <= 4
            
            # Température intérieure
            current_temp = generate_room_temperature(
                ext_temp,
                current_temp,
                profile,
                current_date.hour,
                is_heating
            )
            
            timestamps.append(current_date)
            temperatures.append(current_temp)
            external_temps.append(ext_temp)
            
            current_date += timedelta(minutes=interval_minutes)
        
        return pd.DataFrame({
            'Timestamp': timestamps,
            'Temperature_Celsius(°C)': temperatures,
            'External_Temp(°C)': external_temps
        })
    
    def train_model(self):
        """Lance l'entraînement du modèle"""
        # Vérifier si des fichiers sont importés
        has_imported_files = any(room.use_real_data and room.csv_path for room in self.rooms)
        
        if has_imported_files:
            # Copier automatiquement les fichiers importés avant l'entraînement
            self.log("="*60)
            self.log("📋 PRÉPARATION DES DONNÉES")
            self.log("="*60)
            
            for i, room in enumerate(self.rooms, 1):
                if room.use_real_data and room.csv_path:
                    if not os.path.exists(room.csv_path):
                        self.log(f"⚠️  Room {i}: Fichier introuvable: {room.csv_path}")
                        continue
                    
                    try:
                        df = pd.read_csv(room.csv_path)
                        output_file = f"data/Room{room.room_id}_data.csv"
                        
                        # Créer le dossier data s'il n'existe pas
                        os.makedirs("data", exist_ok=True)
                        
                        df.to_csv(output_file, index=False)
                        self.log(f"✓ Room {i}: {len(df)} lignes → {output_file}")
                    except Exception as e:
                        self.log(f"❌ Room {i}: Erreur - {str(e)}")
            
            self.log("")
        
        response = messagebox.askyesno(
            "Entraîner le Modèle",
            "Lancer l'entraînement du modèle avec les données actuelles?\n\n"
            "Cela peut prendre plusieurs minutes."
        )
        
        if response:
            self.log("="*60)
            self.log("🤖 LANCEMENT DE L'ENTRAÎNEMENT")
            self.log("="*60)
            self.log("Exécution de train_model_with_date.py...")
            self.log("")
            
            # Lancer dans un thread pour ne pas bloquer l'UI
            import threading
            thread = threading.Thread(target=self._train_model_thread)
            thread.start()
    
    def _train_model_thread(self):
        """Thread pour l'entraînement"""
        import subprocess
        import sys
        
        try:
            # Trouver l'interpréteur Python (venv si disponible, sinon système)
            venv_python = Path(".venv/Scripts/python.exe")
            if venv_python.exists():
                python_exe = str(venv_python.absolute())
            else:
                python_exe = sys.executable
            
            # Lancer le script
            process = subprocess.Popen(
                [python_exe, "train_model_with_date.py"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )
            
            # Lire la sortie ligne par ligne de manière sécurisée
            for line in process.stdout:
                try:
                    # Utiliser after() pour mettre à jour depuis le thread principal
                    self.root.after(0, self._safe_log, line.rstrip())
                except:
                    # Si l'interface est fermée, continuer quand même
                    pass
            
            process.wait()
            
            if process.returncode == 0:
                try:
                    self.root.after(0, self._safe_log, "")
                    self.root.after(0, self._safe_log, "="*60)
                    self.root.after(0, self._safe_log, "✅ ENTRAÎNEMENT TERMINÉ AVEC SUCCÈS")
                    self.root.after(0, self._safe_log, "="*60)
                    
                    # Lancer automatiquement l'export CSV vers Arduino
                    self.root.after(0, self._export_csv_to_arduino_thread)
                except:
                    pass
            else:
                try:
                    self.root.after(0, self._safe_log, "")
                    self.root.after(0, self._safe_log, "❌ ERREUR LORS DE L'ENTRAÎNEMENT")
                    self.root.after(0, lambda: messagebox.showerror("Erreur", 
                        "Erreur lors de l'entraînement.\nConsultez le journal pour les détails."))
                except:
                    pass
        
        except Exception as e:
            try:
                self.root.after(0, self._safe_log, f"❌ EXCEPTION: {e}")
                self.root.after(0, lambda: messagebox.showerror("Erreur", f"Erreur:\n{e}"))
            except:
                pass
    
    def _export_csv_to_arduino_thread(self):
        """Export automatique des CSV vers Arduino après entraînement"""
        import subprocess
        import sys
        
        try:
            self._safe_log("")
            self._safe_log("="*60)
            self._safe_log("📊 EXPORT CSV VERS ARDUINO")
            self._safe_log("="*60)
            
            # Vérifier si les fichiers CSV existent
            csv_files = [
                "data/Room1_data.csv",
                "data/Room2_data.csv", 
                "data/Room3_data.csv"
            ]
            
            missing = [f for f in csv_files if not os.path.exists(f)]
            if missing:
                self._safe_log(f"⚠️  Fichiers CSV manquants: {', '.join(missing)}")
                self._safe_log("⚠️  Export annulé - page DONNÉES CSV utilisera les anciennes données")
                self.root.after(0, lambda: messagebox.showinfo("Succès", 
                    "Modèle entraîné avec succès!\n\n⚠️ Export CSV ignoré (fichiers manquants)\n\nVous pouvez maintenant uploader sur M5Stack."))
                return
            
            # Trouver l'interpréteur Python
            venv_python = Path(".venv/Scripts/python.exe")
            if venv_python.exists():
                python_exe = str(venv_python.absolute())
            else:
                python_exe = sys.executable
            
            # Lancer le script d'export
            process = subprocess.Popen(
                [python_exe, "export_csv_to_arduino_v2.py"],  # Version scalable (100+ rooms)
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )
            
            # Lire la sortie
            for line in process.stdout:
                try:
                    self.root.after(0, self._safe_log, line.rstrip())
                except:
                    pass
            
            process.wait()
            
            if process.returncode == 0:
                self._safe_log("")
                self._safe_log("="*60)
                self._safe_log("✅ EXPORT CSV TERMINÉ")
                self._safe_log("="*60)
                self._safe_log("📌 Fichier généré: M5Stack_Temperature_Prediction/RoomPredictor/csv_data.h")
                self._safe_log("📌 Prochaine étape: Recompiler et téléverser le code Arduino")
                self._safe_log("")
                
                self.root.after(0, lambda: messagebox.showinfo("Succès", 
                    "✅ Modèle entraîné avec succès!\n"
                    "✅ Données CSV exportées vers csv_data.h\n\n"
                    "📌 Prochaine étape:\n"
                    "   1. Ouvrir Arduino IDE\n"
                    "   2. Compiler RoomPredictor.ino\n"
                    "   3. Téléverser vers M5Stack\n\n"
                    "La page DONNÉES CSV affichera vos vraies données!"))
            else:
                self._safe_log("")
                self._safe_log("❌ ERREUR LORS DE L'EXPORT CSV")
                
                self.root.after(0, lambda: messagebox.showwarning("Export incomplet", 
                    "Modèle entraîné avec succès!\n\n"
                    "⚠️ Erreur lors de l'export CSV vers Arduino.\n"
                    "Vous pouvez uploader le modèle, mais la page DONNÉES CSV\n"
                    "utilisera les anciennes données."))
        
        except Exception as e:
            self._safe_log(f"❌ EXCEPTION EXPORT: {e}")
            self.root.after(0, lambda: messagebox.showwarning("Export incomplet", 
                f"Modèle entraîné avec succès!\n\n⚠️ Erreur export CSV:\n{e}"))
    
    def _safe_log(self, message):
        """Méthode sécurisée pour logger depuis le thread principal"""
        try:
            timestamp = datetime.now().strftime("%H:%M:%S")
            self.log_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.log_text.see(tk.END)
        except:
            pass
    
    def manual_export_to_arduino(self):
        """Export manuel des CSV vers Arduino (bouton dédié)"""
        # Vérifier si les fichiers CSV existent
        csv_files = [
            "data/Room1_data.csv",
            "data/Room2_data.csv", 
            "data/Room3_data.csv"
        ]
        
        missing = [f for f in csv_files if not os.path.exists(f)]
        if missing:
            messagebox.showerror("Fichiers manquants", 
                f"Les fichiers CSV suivants sont manquants:\n\n" + 
                "\n".join(missing) + 
                "\n\nGénérez ou importez des données d'abord.")
            return
        
        response = messagebox.askyesno(
            "Export vers Arduino",
            "Exporter les données CSV vers le M5Stack?\n\n"
            "Cela générera csv_data.h avec vos vraies données.\n"
            "Après l'export, vous devrez recompiler et téléverser\n"
            "le code Arduino pour voir les changements."
        )
        
        if response:
            self.log("="*60)
            self.log("📤 EXPORT MANUEL VERS ARDUINO")
            self.log("="*60)
            
            # Lancer dans un thread
            import threading
            thread = threading.Thread(target=self._export_csv_to_arduino_thread)
            thread.start()
    
    def visualize_csv_data(self):
        """Affiche une fenêtre avec les graphiques des données CSV"""
        # Vérifier si des fichiers CSV existent
        csv_files = []
        for i in range(1, len(self.rooms) + 1):
            csv_path = f"data/Room{i}_data.csv"
            if os.path.exists(csv_path):
                csv_files.append((i, csv_path))
        
        if not csv_files:
            messagebox.showwarning("Aucune donnée", 
                "Aucun fichier CSV trouvé dans le dossier data/\n\n"
                "Générez ou importez des données d'abord.")
            return
        
        # Créer fenêtre de visualisation
        viz_window = tk.Toplevel(self.root)
        viz_window.title("📊 Visualisation des Données CSV")
        viz_window.geometry("1200x700")
        
        # Variable pour stocker la figure courante
        current_fig = [None]  # Liste pour pouvoir modifier dans update_graph
        
        # Frame pour les contrôles
        control_frame = ttk.Frame(viz_window)
        control_frame.pack(fill='x', padx=10, pady=5)
        
        ttk.Label(control_frame, text="Nombre de points à afficher:", 
                 font=('Arial', 10)).pack(side='left', padx=5)
        
        points_var = tk.IntVar(value=1000)
        points_spinbox = ttk.Spinbox(control_frame, from_=100, to=10000, 
                                     increment=100, textvariable=points_var, width=10)
        points_spinbox.pack(side='left', padx=5)
        
        # Frame pour le graphique
        graph_frame = ttk.Frame(viz_window)
        graph_frame.pack(fill='both', expand=True, padx=10, pady=5)
        
        # Frame pour les statistiques
        stats_frame = ttk.LabelFrame(viz_window, text="📈 Statistiques", padding="10")
        stats_frame.pack(fill='x', padx=10, pady=5)
        
        stats_text = scrolledtext.ScrolledText(stats_frame, height=8, width=80, 
                                               font=('Consolas', 9))
        stats_text.pack(fill='both', expand=True)
        
        def update_graph():
            """Met à jour le graphique"""
            try:
                # Effacer ancien graphique
                for widget in graph_frame.winfo_children():
                    widget.destroy()
                
                # Charger données
                n_points = points_var.get()
                
                fig = Figure(figsize=(12, 6), dpi=100)
                ax = fig.add_subplot(111)
                
                # Sauvegarder la figure pour l'export
                current_fig[0] = fig
                
                colors = ['#E74C3C', '#2ECC71', '#3498DB', '#F39C12', '#9B59B6', '#1ABC9C']
                
                stats_text.delete(1.0, tk.END)
                stats_text.insert(tk.END, "=== STATISTIQUES DES DONNÉES ===\n\n")
                
                for idx, (room_num, csv_path) in enumerate(csv_files):
                    df = pd.read_csv(csv_path)
                    
                    # Trouver colonne timestamp et température
                    timestamp_col = [c for c in df.columns if 'timestamp' in c.lower()][0]
                    temp_col = [c for c in df.columns if 'temp' in c.lower() and 'celsius' in c.lower()][0]
                    
                    df[timestamp_col] = pd.to_datetime(df[timestamp_col])
                    
                    # Limiter aux derniers N points
                    df_plot = df.tail(n_points)
                    
                    # Tracer
                    color = colors[idx % len(colors)]
                    room_name = self.rooms[idx].name if idx < len(self.rooms) else f"Room {room_num}"
                    ax.plot(df_plot[timestamp_col], df_plot[temp_col], 
                           label=room_name, color=color, linewidth=1.5, alpha=0.8)
                    
                    # Statistiques
                    stats_text.insert(tk.END, f"{room_name} ({len(df)} points totaux):\n")
                    stats_text.insert(tk.END, f"  Min: {df[temp_col].min():.2f}°C\n")
                    stats_text.insert(tk.END, f"  Max: {df[temp_col].max():.2f}°C\n")
                    stats_text.insert(tk.END, f"  Moyenne: {df[temp_col].mean():.2f}°C\n")
                    stats_text.insert(tk.END, f"  Écart-type: {df[temp_col].std():.2f}°C\n")
                    stats_text.insert(tk.END, f"  Période: {df[timestamp_col].min()} → {df[timestamp_col].max()}\n\n")
                
                ax.set_xlabel('Date/Heure', fontsize=11, fontweight='bold')
                ax.set_ylabel('Température (°C)', fontsize=11, fontweight='bold')
                ax.set_title(f'Températures - Derniers {n_points} points', 
                            fontsize=13, fontweight='bold')
                ax.legend(fontsize=10, loc='best')
                ax.grid(True, alpha=0.3, linestyle='--')
                
                # Rotation des labels X
                fig.autofmt_xdate()
                
                fig.tight_layout()
                
                # Intégrer dans Tkinter
                canvas = FigureCanvasTkAgg(fig, master=graph_frame)
                canvas.draw()
                canvas.get_tk_widget().pack(fill='both', expand=True)
                
            except Exception as e:
                messagebox.showerror("Erreur", f"Erreur lors de la visualisation:\n{str(e)}")
                self.log(f"❌ Erreur visualisation: {e}")
        
        def export_graph():
            """Exporte le graphique en PNG"""
            if current_fig[0] is None:
                messagebox.showerror("Erreur", "Aucun graphique à exporter")
                return
            
            filepath = filedialog.asksaveasfilename(
                defaultextension=".png",
                filetypes=[("PNG", "*.png"), ("PDF", "*.pdf"), ("SVG", "*.svg")]
            )
            
            if filepath:
                current_fig[0].savefig(filepath, dpi=300, bbox_inches='tight')
                messagebox.showinfo("Succès", f"Graphique exporté:\n{filepath}")
                self.log(f"✓ Graphique exporté: {filepath}")
        
        # Bouton refresh
        ttk.Button(control_frame, text="🔄 Actualiser", 
                  command=update_graph).pack(side='left', padx=5)
        
        ttk.Button(control_frame, text="💾 Exporter PNG", 
                  command=export_graph).pack(side='left', padx=5)
        
        # Afficher graphique initial
        update_graph()

def main():
    """Point d'entrée principal"""
    root = tk.Tk()
    app = DataGeneratorGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
