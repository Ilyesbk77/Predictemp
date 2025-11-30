# 🚀 Guide de Mise en Ligne sur GitHub

## Prérequis

1. **Compte GitHub créé** : https://github.com/signup
2. **Git installé** sur votre PC : https://git-scm.com/download/win
3. **Projet propre** : Fichiers inutiles déjà supprimés ✅

---

## Étape 1 : Créer un Nouveau Repository sur GitHub

### Via l'Interface Web

1. Allez sur https://github.com
2. Cliquez sur le bouton **"+"** en haut à droite → **"New repository"**
3. Remplissez les informations :
   - **Repository name** : `Predictemp` (ou `M5Stack-Temperature-Prediction`)
   - **Description** : `🌡️ Système de prédiction de température multi-pièces avec IA sur M5Stack`
   - **Visibilité** : 
     - ✅ **Public** (recommandé pour portfolio étudiant)
     - ou **Private** (si projet confidentiel)
   - **Ne pas** cocher "Initialize with README" (vous en avez déjà un)
4. Cliquez sur **"Create repository"**

### Récupérez l'URL de votre Repository

Après création, GitHub affiche une URL comme :
```
https://github.com/VOTRE-USERNAME/Predictemp.git
```

**Copiez cette URL**, vous en aurez besoin.

---

## Étape 2 : Initialiser Git Localement

Ouvrez **PowerShell** dans le dossier `D:\Dev\Predictemp` et exécutez :

### 2.1 Initialiser le Repository Git

```powershell
# Initialiser Git dans le dossier
git init

# Vérifier que .gitignore est bien présent
Get-Content .gitignore
```

### 2.2 Configurer votre Identité Git (si première fois)

```powershell
# Remplacez par votre nom et email GitHub
git config --global user.name "Votre Nom"
git config --global user.email "votre.email@example.com"

# Vérifier la configuration
git config --list
```

### 2.3 Ajouter tous les Fichiers

```powershell
# Ajouter tous les fichiers au staging (sauf ceux dans .gitignore)
git add .

# Vérifier les fichiers qui seront commités
git status
```

**Fichiers Ignorés Automatiquement** (grâce à `.gitignore`) :
- ❌ `.venv/` (environnement virtuel Python)
- ❌ `__pycache__/` (cache Python)
- ❌ `*.h5` (modèles TensorFlow - trop gros)
- ❌ `.vscode/` (configuration IDE)

**Fichiers Inclus** :
- ✅ `data/Room*.csv` (données de 10 pièces)
- ✅ Scripts Python (`.py`)
- ✅ Firmware Arduino (`.ino`, `.h`)
- ✅ Documentation (`.md`)
- ✅ `requirements.txt`

### 2.4 Créer le Premier Commit

```powershell
# Créer un commit avec message descriptif
git commit -m "🎉 Initial commit - Système de prédiction température multi-pièces avec IA"

# Vérifier que le commit est créé
git log --oneline
```

---

## Étape 3 : Connecter au Repository GitHub

### 3.1 Ajouter le Remote

```powershell
# Remplacez VOTRE-USERNAME par votre nom d'utilisateur GitHub
git remote add origin https://github.com/VOTRE-USERNAME/Predictemp.git

# Vérifier que le remote est ajouté
git remote -v
```

### 3.2 Renommer la Branche en 'main'

```powershell
# GitHub utilise 'main' par défaut (pas 'master')
git branch -M main
```

### 3.3 Pousser le Code sur GitHub

```powershell
# Push vers GitHub (première fois)
git push -u origin main
```

**Si demande d'authentification** :
- **Username** : Votre nom d'utilisateur GitHub
- **Password** : Utilisez un **Personal Access Token** (pas votre mot de passe)

---

## Étape 4 : Créer un Personal Access Token (si nécessaire)

Si Git demande un mot de passe et que ça ne fonctionne pas :

1. Allez sur https://github.com/settings/tokens
2. Cliquez **"Generate new token"** → **"Generate new token (classic)"**
3. Donnez un nom : `Git Push Access`
4. Cochez : `repo` (Full control of private repositories)
5. Cliquez **"Generate token"**
6. **COPIEZ LE TOKEN** (vous ne le reverrez plus !)
7. Utilisez ce token comme **mot de passe** lors du `git push`

---

## Étape 5 : Vérifier sur GitHub

1. Allez sur `https://github.com/VOTRE-USERNAME/Predictemp`
2. Vous devriez voir :
   - ✅ Tous vos fichiers
   - ✅ README.md affiché automatiquement
   - ✅ Structure de dossiers
   - ✅ Historique des commits

---

## 🔄 Workflow pour Futures Modifications

### Après avoir modifié des fichiers

```powershell
# 1. Vérifier les fichiers modifiés
git status

# 2. Ajouter les modifications
git add .

# 3. Créer un commit
git commit -m "📝 Description des changements"

# 4. Pousser sur GitHub
git push
```

### Exemples de Messages de Commit

```powershell
# Amélioration du modèle
git commit -m "✨ Amélioration MAE de 2.1°C à 1.79°C"

# Correction de bug
git commit -m "🐛 Fix corruption mémoire W0[3][32] → W0[5][32]"

# Nouvelle fonctionnalité
git commit -m "🚀 Ajout support 100+ pièces"

# Documentation
git commit -m "📚 Ajout rapport académique complet"

# Optimisation
git commit -m "⚡ Réduction latence inférence 5ms → 1.8ms"
```

---

## 📋 Commandes Complètes (Copier-Coller)

### Première Mise en Ligne (À faire UNE FOIS)

```powershell
# 1. Initialiser Git
git init

# 2. Configurer identité (remplacez les valeurs)
git config --global user.name "Votre Nom"
git config --global user.email "votre.email@example.com"

# 3. Ajouter tous les fichiers
git add .

# 4. Premier commit
git commit -m "🎉 Initial commit - Système de prédiction température multi-pièces avec IA"

# 5. Connecter à GitHub (remplacez VOTRE-USERNAME)
git remote add origin https://github.com/VOTRE-USERNAME/Predictemp.git

# 6. Renommer branche
git branch -M main

# 7. Push initial
git push -u origin main
```

### Mises à Jour Futures (À chaque modification)

```powershell
# 1. Vérifier les changements
git status

# 2. Ajouter les modifications
git add .

# 3. Commit avec message
git commit -m "📝 Description des changements"

# 4. Push vers GitHub
git push
```

---

## 🎨 Améliorer votre README.md

Pour rendre votre projet attractif sur GitHub, ajoutez au début de `README.md` :

### Badges

```markdown
# 🌡️ Predictemp - Système de Prédiction de Température Multi-Pièces

![Python](https://img.shields.io/badge/Python-3.10-blue)
![TensorFlow](https://img.shields.io/badge/TensorFlow-2.18-orange)
![Arduino](https://img.shields.io/badge/Arduino-C++-00979D)
![M5Stack](https://img.shields.io/badge/M5Stack-TABS-red)
![License](https://img.shields.io/badge/License-MIT-green)
![MAE](https://img.shields.io/badge/MAE-1.79°C-success)
```

### Screenshots

Ajoutez des images de votre M5Stack dans un dossier `screenshots/` :

```markdown
## 📸 Aperçu

![Interface M5Stack](screenshots/m5stack-interface.jpg)
![Graphiques Prédiction](screenshots/predictions.jpg)
```

---

## 🔍 Vérifier que `.gitignore` Fonctionne

### Avant le premier commit, vérifiez :

```powershell
# Lister les fichiers qui seront commités
git status

# Si vous voyez .venv/ ou __pycache__/ → PROBLÈME
# Ils doivent être ignorés
```

### Si des fichiers indésirables apparaissent :

```powershell
# Supprimer du cache Git (pas du disque)
git rm -r --cached .venv/
git rm -r --cached __pycache__/

# Re-ajouter tout
git add .

# Commit
git commit -m "🔧 Fix .gitignore"
```

---

## 📦 Taille du Repository

### Fichiers Volumineux

- `data/Room*.csv` : ~5 MB (10 pièces × 365 jours) ✅
- `M5Stack_Temperature_Prediction/RoomPredictor/csv_data.h` : ~132 KB ✅
- `M5Stack_Temperature_Prediction/RoomPredictor/neural_weights.h` : ~24 KB ✅

**Total estimé** : ~10-15 MB (acceptable pour GitHub)

### Si Trop Gros (> 100 MB)

GitHub a une limite de 100 MB par fichier. Si nécessaire :

```powershell
# Installer Git LFS (Large File Storage)
git lfs install

# Marquer les gros fichiers
git lfs track "*.csv"
git lfs track "*.h"

# Ajouter .gitattributes
git add .gitattributes

# Commit
git commit -m "🔧 Ajout Git LFS pour gros fichiers"
```

---

## 🌟 Bonus : GitHub Pages (Site Web Gratuit)

Pour publier votre documentation en ligne :

1. Allez sur GitHub → **Settings** → **Pages**
2. **Source** : Deploy from a branch
3. **Branch** : `main` → `/ (root)`
4. Cliquez **Save**

Votre README sera accessible sur :
```
https://VOTRE-USERNAME.github.io/Predictemp/
```

---

## ✅ Checklist Complète

- [ ] Compte GitHub créé
- [ ] Git installé sur PC
- [ ] Repository créé sur GitHub
- [ ] URL du repository copiée
- [ ] `git init` exécuté
- [ ] Identité Git configurée (`user.name`, `user.email`)
- [ ] `git add .` exécuté
- [ ] Premier commit créé
- [ ] Remote ajouté (`git remote add origin`)
- [ ] Branche renommée en `main`
- [ ] Code poussé sur GitHub (`git push`)
- [ ] Vérification sur GitHub.com ✅

---

## 🆘 Résolution de Problèmes

### Erreur : "remote origin already exists"

```powershell
# Supprimer l'ancien remote
git remote remove origin

# Re-ajouter le bon
git remote add origin https://github.com/VOTRE-USERNAME/Predictemp.git
```

### Erreur : "Authentication failed"

1. Utilisez un **Personal Access Token** (pas mot de passe)
2. Ou configurez SSH : https://docs.github.com/en/authentication/connecting-to-github-with-ssh

### Erreur : "Push rejected"

```powershell
# Forcer le push (ATTENTION : écrase l'historique distant)
git push -u origin main --force
```

### Fichiers Sensibles Commités par Erreur

```powershell
# Supprimer du Git (pas du disque)
git rm --cached fichier_sensible.txt

# Ajouter à .gitignore
echo "fichier_sensible.txt" >> .gitignore

# Commit
git add .gitignore
git commit -m "🔒 Suppression fichier sensible"
git push
```

---

## 📚 Ressources Utiles

- **Documentation Git** : https://git-scm.com/doc
- **GitHub Guides** : https://guides.github.com/
- **Markdown Cheatsheet** : https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet
- **Emoji Commit** : https://gitmoji.dev/

---

**Temps Estimé** : 10-15 minutes pour la première mise en ligne 🚀

**Votre projet sera visible publiquement** et pourra être ajouté à votre **portfolio étudiant** !
