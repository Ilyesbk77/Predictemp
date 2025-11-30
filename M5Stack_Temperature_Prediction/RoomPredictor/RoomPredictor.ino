#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "neural_weights.h"
#include "csv_data.h"  // Données CSV réelles

// ============================================================================
// FORWARD DECLARATIONS (Déclarations anticipées pour éviter erreurs compilation)
// ============================================================================
void predict_rooms(float temp_ext, float humidity, int day, int month, float* output);

// ============================================================================
// CONFIGURATION WiFi & API
// ============================================================================

// WiFi credentials (simple)
char wifi_ssid[32] = "";
char wifi_password[64] = "";

// API Open-Meteo avec HTTPS
const char* WEATHER_API = "https://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35&current=temperature_2m";
const char* WEATHER_FORECAST_API = "https://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35&hourly=temperature_2m&forecast_days=1";
const char* TIME_API = "https://worldtimeapi.org/api/timezone/Europe/Paris";

// Rate Limiting pour APIs
unsigned long last_api_call = 0;
const unsigned long API_COOLDOWN = 10000; // 10 secondes entre appels

// Offline Queue System - Retry automatique en cas d'échec réseau
struct QueuedRequest {
  char url[256];
  unsigned long timestamp;
  int retry_count;
};

#define MAX_QUEUE_SIZE 5
QueuedRequest offline_queue[MAX_QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;
int queue_count = 0;

// Performance Monitoring
struct PerformanceMetrics {
  unsigned long last_api_latency = 0;
  unsigned long last_inference_time = 0;
  unsigned long total_free_heap = 0;
  unsigned long min_free_heap = ULONG_MAX;
  int api_success_count = 0;
  int api_failure_count = 0;
};
PerformanceMetrics perf;

// ============================================================================
// VARIABLES
// ============================================================================

float current_temp = 20.0f;
float real_temp = 0.0f;  // Température récupérée d'internet
float predictions[3];     // Room1, Room2, Room3
int current_page = 0;     // 0=Menu, 1=Input, 2=Result, 3=Real, 4=Graph Manual, 5=Prediction Temp, 6=WiFi Config, 7=Keyboard, 8=Saved Networks, 9=Period Select, 10=Historical Data
bool wifi_connected = false;

// Sélection de période pour les graphiques API
int selected_period = 0;  // 0=6h, 1=12h, 2=24h, 3=1sem
const char* period_labels[] = {"6h", "12h", "24h", "1 semaine"};
const char* period_api_params[] = {"6h", "12h", "24h", "1w"};

// Sélection pour graphiques CSV historiques
int csv_period = 0;  // 0=1jour, 1=1sem, 2=1mois, 3=3mois
const char* csv_period_labels[] = {"1 jour", "1 semaine", "1 mois", "3 mois"};
int csv_samples[] = {48, 168, 720, 2160};  // Nombre d'échantillons (1 mesure / 30min)
int csv_scroll_offset = 0;  // Décalage pour voir différentes périodes

// Sélecteur de chambre pour Prediction Temp (TAB5)
int selected_room_view = 0;  // 0=Toutes, 1=Room1, 2=Room2, 3=Room3, etc.

// Fonction pour obtenir le label de la room sélectionnée
String getRoomViewLabel(int view_idx) {
  if (view_idx == 0) return "TOUTES";
  return "ROOM " + String(view_idx);
}
// MAX_ROOMS est maintenant défini dynamiquement dans csv_data.h comme CSV_NUM_ROOMS

// Scanner WiFi
#define MAX_NETWORKS 20
String scanned_ssids[MAX_NETWORKS];
int scanned_rssi[MAX_NETWORKS];
int scanned_count = 0;
int selected_network = -1;
int wifi_scroll_offset = 0;  // Décalage pour le défilement (0, 1, 2...)

// Clavier virtuel
String keyboard_input = "";
bool keyboard_shift = false;
int keyboard_mode = 0; // 0=lowercase, 1=uppercase, 2=numbers

// Date/Heure pour prédiction manuelle (modifiable par l'utilisateur)
int manual_day = 25;      // Jour (1-31)
int manual_month = 11;    // Mois (1-12)
int manual_hour = 14;     // Heure (0-23)
int manual_minute = 30;   // Minute (0-59)
String manual_date = "25/11";  // Format DD/MM (généré)
String manual_time = "14:30";  // Format HH:MM (généré)

// ✅ CACHE API pour limiter les appels réseau
struct APICache {
  String response;          // Dernière réponse API
  unsigned long timestamp;  // Horodatage en millis()
  int cached_period;        // Période mise en cache (0-4)
  bool valid;               // Cache valide ?
};

APICache weather_cache = {"", 0, -1, false};
const unsigned long CACHE_DURATION_MS = 300000;  // 5 minutes en millisecondes

bool is_cache_valid() {
  if (!weather_cache.valid || weather_cache.response.length() == 0) {
    return false;
  }
  
  // Vérifier que la période demandée est la même que celle en cache
  if (weather_cache.cached_period != selected_period) {
    Serial.println("[CACHE] Période différente, invalidation");
    return false;
  }
  
  unsigned long elapsed = millis() - weather_cache.timestamp;
  
  // Gestion overflow millis() (après ~50 jours)
  if (elapsed > 4000000000UL) {
    Serial.println("[CACHE] Overflow millis() détecté, invalidation");
    return false;
  }
  
  return elapsed < CACHE_DURATION_MS;
}

void invalidate_cache() {
  weather_cache.valid = false;
  weather_cache.response = "";
  weather_cache.cached_period = -1;
  Serial.println("[CACHE] Cache invalidé");
}


// Historique MANUEL (10 dernières prédictions manuelles)
#define HISTORY_SIZE 10
float history_manual_ext[HISTORY_SIZE];
float history_manual_r1[HISTORY_SIZE];
float history_manual_r2[HISTORY_SIZE];
float history_manual_r4[HISTORY_SIZE];
String history_manual_labels[HISTORY_SIZE];  // Labels date/heure
int history_manual_count = 0;

// Historique API (jusqu'à 16 jours = 16 points)
#define API_HISTORY_SIZE 16
float history_api_ext[API_HISTORY_SIZE];
float history_api_rooms[API_HISTORY_SIZE][NUM_ROOMS];  // Tableau 2D pour toutes les rooms
String history_api_labels[API_HISTORY_SIZE];  // Labels (heures ou dates)
int history_api_count = 0;

// ============================================================================
// SECURITY & API HELPERS
// ============================================================================

// Certificats CA racine pour Certificate Pinning (prévention MITM)
// ISRG Root X1 (Let's Encrypt) - utilisé par api.open-meteo.com
const char* ISRG_ROOT_X1_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n" \
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n" \
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n" \
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n" \
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n" \
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n" \
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n" \
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n" \
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n" \
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n" \
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n" \
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n" \
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n" \
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n" \
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n" \
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n" \
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n" \
"-----END CERTIFICATE-----\n";

// DigiCert Global Root CA - utilisé par worldtimeapi.org
const char* DIGICERT_GLOBAL_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n";

// Rate limiting check
bool check_rate_limit() {
  if (millis() - last_api_call < API_COOLDOWN) {
    Serial.println("[SECURITY] Rate limit: attendez 30s entre les appels API");
    return false;
  }
  last_api_call = millis();
  return true;
}

// HTTPS GET sécurisé avec validation et Certificate Pinning
bool secure_https_get(const char* url, String &response) {
  if (!check_rate_limit()) {
    return false;
  }
  
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    Serial.println("[ERROR] Impossible de créer WiFiClientSecure");
    return false;
  }
  
  // Certificate Pinning - sélectionner le bon CA selon l'URL
  if (strstr(url, "open-meteo.com")) {
    client->setCACert(ISRG_ROOT_X1_CA);
    Serial.println("[SECURITY] Certificate Pinning: ISRG Root X1 (Let's Encrypt)");
  } else if (strstr(url, "worldtimeapi.org")) {
    client->setCACert(DIGICERT_GLOBAL_ROOT_CA);
    Serial.println("[SECURITY] Certificate Pinning: DigiCert Global Root CA");
  } else {
    // Fallback pour autres APIs (mode insecure)
    client->setInsecure();
    Serial.println("[WARNING] API inconnue - mode insecure");
  }
  
  HTTPClient https;
  bool success = false;
  unsigned long start_time = millis(); // Mesure latence
  int max_retries = 3;  // ✅ Nombre de tentatives
  int retry_count = 0;
  
  Serial.print("[HTTPS] Connexion à: ");
  Serial.println(url);
  
  // ✅ Boucle de retry
  while (retry_count < max_retries && !success) {
    if (retry_count > 0) {
      Serial.printf("[RETRY] Tentative %d/%d après échec...\n", retry_count + 1, max_retries);
      delay(1000 * retry_count);  // Délai exponentiel: 1s, 2s, 3s
    }
    
    if (https.begin(*client, url)) {
      https.setTimeout(15000); // ✅ Timeout augmenté à 15s
      
      int httpCode = https.GET();
      perf.last_api_latency = millis() - start_time; // Enregistrer latence
      
      Serial.print("[HTTPS] Code retour: ");
      Serial.println(httpCode);
      Serial.printf("[PERF] Latence API: %lu ms\n", perf.last_api_latency);
      
      if (httpCode == HTTP_CODE_OK) {
        response = https.getString();
        
        // Validation basique de la réponse
        if (response.length() > 0 && response.length() < 50000) {
          success = true;
          perf.api_success_count++;
          Serial.printf("[HTTPS] ✓ Données reçues et validées (%d bytes)\n", response.length());
        } else {
          Serial.println("[SECURITY] Réponse invalide (taille suspecte)");
          perf.api_failure_count++;
          retry_count++;
        }
      } else if (httpCode == HTTP_CODE_REQUEST_TIMEOUT || httpCode == -1) {
        Serial.printf("[ERROR] Timeout (code: %d), nouvelle tentative...\n", httpCode);
        perf.api_failure_count++;
        retry_count++;
      } else if (httpCode >= 500 && httpCode < 600) {
        Serial.printf("[ERROR] Erreur serveur (code: %d), nouvelle tentative...\n", httpCode);
        perf.api_failure_count++;
        retry_count++;
      } else {
        Serial.printf("[ERROR] HTTPS failed, code: %d (pas de retry)\n", httpCode);
        perf.api_failure_count++;
        break;  // Erreur définitive (4xx), pas de retry
      }
      
      https.end();
    } else {
      Serial.println("[ERROR] Impossible de démarrer connexion HTTPS");
      perf.api_failure_count++;
      retry_count++;
    }
  }
  
  if (!success && retry_count >= max_retries) {
    Serial.println("[ERROR] ✗ Échec après 3 tentatives");
  }
  
  delete client;
  
  // Monitoring heap memory
  perf.total_free_heap = ESP.getFreeHeap();
  if (perf.total_free_heap < perf.min_free_heap) {
    perf.min_free_heap = perf.total_free_heap;
  }
  Serial.printf("[PERF] RAM libre: %lu bytes (min: %lu)\n", perf.total_free_heap, perf.min_free_heap);
  
  return success;
}

// Offline Queue - Ajouter requête échouée
void queue_add_request(const char* url) {
  if (queue_count >= MAX_QUEUE_SIZE) {
    Serial.println("[QUEUE] File pleine - requête la plus ancienne supprimée");
    queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
    queue_count--;
  }
  
  strncpy(offline_queue[queue_tail].url, url, 255);
  offline_queue[queue_tail].url[255] = '\0';
  offline_queue[queue_tail].timestamp = millis();
  offline_queue[queue_tail].retry_count = 0;
  
  queue_tail = (queue_tail + 1) % MAX_QUEUE_SIZE;
  queue_count++;
  
  Serial.printf("[QUEUE] Requête ajoutée (total: %d/%d)\n", queue_count, MAX_QUEUE_SIZE);
}

// Offline Queue - Retry requêtes en attente
void queue_process() {
  if (queue_count == 0 || WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  Serial.printf("[QUEUE] Traitement de %d requête(s) en attente\n", queue_count);
  
  for (int i = 0; i < queue_count; i++) {
    int index = (queue_head + i) % MAX_QUEUE_SIZE;
    QueuedRequest* req = &offline_queue[index];
    
    // Limite retry à 3 tentatives
    if (req->retry_count >= 3) {
      Serial.printf("[QUEUE] Requête abandonnée après 3 tentatives: %s\n", req->url);
      queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
      queue_count--;
      continue;
    }
    
    String response;
    if (secure_https_get(req->url, response)) {
      Serial.printf("[QUEUE] Requête réussie: %s\n", req->url);
      queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
      queue_count--;
    } else {
      req->retry_count++;
      Serial.printf("[QUEUE] Échec retry %d/3 pour: %s\n", req->retry_count, req->url);
    }
    
    delay(1000); // Éviter surcharge réseau
  }
}

// Validation de température
bool validate_temperature(float temp) {
  if (temp < -50.0 || temp > 60.0) {
    Serial.printf("[SECURITY] Température invalide: %.1f°C (hors plage -50 à 60)\n", temp);
    return false;
  }
  if (isnan(temp) || isinf(temp)) {
    Serial.println("[SECURITY] Température NaN ou Infinie détectée");
    return false;
  }
  return true;
}

// ============================================================================
// WiFi & API FUNCTIONS
// ============================================================================

void scan_wifi_networks() {
  scanned_count = 0;
  
  Serial.println("[WIFI] Démarrage scan WiFi...");
  
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(300, 300);
  M5.Display.println("Scan WiFi...");
  M5.Display.setCursor(280, 330);
  M5.Display.setTextColor(YELLOW);
  M5.Display.println("Patientez SVP...");
  
  // ✅ S'assurer que le WiFi est en mode station
  WiFi.mode(WIFI_STA);
  delay(100);
  
  int n = WiFi.scanNetworks();
  
  Serial.printf("[WIFI] Scan terminé - %d réseau(x) trouvé(s)\n", n);
  
  if (n == 0) {
    scanned_count = 0;
    Serial.println("[WIFI] ⚠ Aucun réseau WiFi détecté!");
    
    M5.Display.setCursor(250, 380);
    M5.Display.setTextColor(RED);
    M5.Display.println("Aucun reseau trouve!");
    delay(1500);
  } else {
    scanned_count = (n > MAX_NETWORKS) ? MAX_NETWORKS : n;
    Serial.printf("[WIFI] Sauvegarde de %d réseau(x) (max: %d)\n", scanned_count, MAX_NETWORKS);
    
    for (int i = 0; i < scanned_count; i++) {
      scanned_ssids[i] = WiFi.SSID(i);
      scanned_rssi[i] = WiFi.RSSI(i);
      Serial.printf("[WIFI]   %d. %s (RSSI: %d dBm)\n", i+1, scanned_ssids[i].c_str(), scanned_rssi[i]);
    }
  }
}

void connect_wifi() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(20, 20);
  M5.Display.println("Connexion WiFi...");
  
  Serial.println("\n[WIFI] ========================================");
  Serial.printf("[WIFI] Tentative connexion WiFi\n");
  Serial.printf("[WIFI] SSID: '%s'\n", wifi_ssid);
  Serial.println("[WIFI] ========================================");
  
  WiFi.begin(wifi_ssid, wifi_password);
  
  int attempts = 0;
  int max_attempts = 20;
  
  M5.Display.setCursor(20, 80);
  M5.Display.setTextColor(CYAN);
  
  while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
    delay(500);
    M5.Display.print(".");
    Serial.print(".");
    attempts++;
    
    if (attempts % 5 == 0) {
      M5.Display.setCursor(20, 110);
      M5.Display.setTextColor(YELLOW);
      M5.Display.printf("Tentative %d/%d...  ", attempts, max_attempts);
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    M5.Display.setCursor(20, 120);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("WiFi Connecte!");
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(20, 150);
    M5.Display.printf("IP: %s", WiFi.localIP().toString().c_str());
    
    Serial.println("[WIFI] ✓ WiFi connecté!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    wifi_connected = false;
    M5.Display.setCursor(20, 120);
    M5.Display.setTextColor(RED);
    M5.Display.setTextSize(3);
    M5.Display.println("ECHEC WIFI");
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setCursor(20, 160);
    M5.Display.println("Verifier SSID/Password");
    
    Serial.println("[WIFI] ✗ Échec connexion");
  }
  
  delay(2000);
}

float get_real_temperature() {
  if (!wifi_connected) {
    Serial.println("[ERROR] WiFi non connecté");
    return -999.0f;
  }
  
  String response;
  if (!secure_https_get(WEATHER_API, response)) {
    Serial.println("[ERROR] Échec requête HTTPS");
    // Ajouter à la queue pour retry automatique
    queue_add_request(WEATHER_API);
    return -999.0f;
  }
  
  // Parser JSON
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.c_str());
    return -999.0f;
  }
  
  // Vérifier que les clés existent
  if (!doc.containsKey("current") || !doc["current"].containsKey("temperature_2m")) {
    Serial.println("[SECURITY] Structure JSON invalide");
    return -999.0f;
  }
  
  float temp = doc["current"]["temperature_2m"];
  
  // Validation de la température
  if (!validate_temperature(temp)) {
    return -999.0f;
  }
  
  Serial.printf("[SUCCESS] Température récupérée: %.1f°C\n", temp);
  return temp;
}

void update_datetime_strings() {
  // Formater la date DD/MM
  char date_buffer[6];
  sprintf(date_buffer, "%02d/%02d", manual_day, manual_month);
  manual_date = String(date_buffer);
  
  // Formater l'heure HH:MM
  char time_buffer[6];
  sprintf(time_buffer, "%02d:%02d", manual_hour, manual_minute);
  manual_time = String(time_buffer);
}

void get_current_datetime() {
  if (!wifi_connected) {
    Serial.println("[ERROR] WiFi non connecté");
    return;
  }
  
  String response;
  if (!secure_https_get(TIME_API, response)) {
    Serial.println("[ERROR] Échec requête date/heure HTTPS");
    // Ajouter à la queue pour retry automatique
    queue_add_request(TIME_API);
    return;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  if (!doc.containsKey("datetime")) {
    Serial.println("[SECURITY] Clé 'datetime' manquante");
    return;
  }
  
  String datetime = doc["datetime"].as<String>();
  
  // Validation format datetime
  if (datetime.length() < 19) {
    Serial.println("[SECURITY] Format datetime invalide");
    return;
  }
  
  // Format: 2025-11-25T14:30:00+01:00
  manual_day = datetime.substring(8, 10).toInt();
  manual_month = datetime.substring(5, 7).toInt();
  manual_hour = datetime.substring(11, 13).toInt();
  manual_minute = datetime.substring(14, 16).toInt();
  
  // Validation des valeurs
  if (manual_day < 1 || manual_day > 31 || manual_month < 1 || manual_month > 12 ||
      manual_hour < 0 || manual_hour > 23 || manual_minute < 0 || manual_minute > 59) {
    Serial.println("[SECURITY] Valeurs date/heure invalides");
    return;
  }
  
  update_datetime_strings();
  Serial.println("[SUCCESS] Date/heure récupérée");
}

// Récupérer prévisions météo et remplir historique API
void fetch_api_forecast() {
  history_api_count = 0;
  String response;  // ✅ Déclaration variable locale
  
  if (!wifi_connected) {
    Serial.println("[ERROR] WiFi non connecté");
    return;
  }
  
  // ✅ Vérifier cache avant appel API
  if (is_cache_valid()) {
    Serial.println("[CACHE] ✓ Utilisation données en cache (économie réseau)");
    response = weather_cache.response;
  } else {
    // Construire URL avec période sélectionnée
    String url = "https://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35";
    
    // Ajouter paramètres selon période
    if (selected_period == 0) {
      // 6h - horaire
      url += "&hourly=temperature_2m&forecast_days=1";
    } else if (selected_period == 1) {
      // 12h - horaire
      url += "&hourly=temperature_2m&forecast_days=1";
    } else if (selected_period == 2) {
      // 24h - horaire
      url += "&hourly=temperature_2m&forecast_days=2";
    } else {
      // 1 semaine - journalier (Open-Meteo gratuit: max 7 jours)
      url += "&daily=temperature_2m_mean&forecast_days=7";
    }
    
    Serial.println("[API] Récupération nouvelles données météo...");
    if (!secure_https_get(url.c_str(), response)) {
      Serial.println("[ERROR] Échec requête prévisions HTTPS");
      return;
    }
    
    // ✅ Sauvegarder dans cache avec la période
    weather_cache.response = response;
    weather_cache.timestamp = millis();
    weather_cache.cached_period = selected_period;
    weather_cache.valid = true;
    Serial.println("[CACHE] ✓ Données sauvegardées en cache (5 min)");
  }
  
  // Parser JSON
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  if (selected_period == 0 || selected_period == 1 || selected_period == 2) {
    // Mode horaire (6h, 12h, 24h)
    if (!doc.containsKey("hourly") || !doc["hourly"].containsKey("temperature_2m")) {
      Serial.println("[SECURITY] Structure JSON hourly invalide");
      return;
    }
    
    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray times = doc["hourly"]["time"];
    
    // Définir le pas selon la période
    int step;
    if (selected_period == 0) step = 3;      // 6h: tous les 3h (2 points)
    else if (selected_period == 1) step = 4; // 12h: tous les 4h (3 points)
    else step = 3;                            // 24h: tous les 3h (8 points)
    
    for (int i = 0; i < API_HISTORY_SIZE && i * step < temps.size(); i++) {
      float temp = temps[i * step];
      
      // Validation température
      if (!validate_temperature(temp)) {
        Serial.printf("[SECURITY] Température #%d ignorée\n", i);
        continue;
      }
      
      String time_str = times[i * step].as<String>();
      if (time_str.length() < 13) continue;
      
      // Extraire date: format "2025-11-26T14:00"
      int day_num = time_str.substring(8, 10).toInt();
      int month_num = time_str.substring(5, 7).toInt();
      int hour = time_str.substring(11, 13).toInt();
      
      // Format label selon période
      if (selected_period == 2 && i > 0 && hour == 0) {
        // 24h: afficher jour quand on change de jour
        history_api_labels[i] = String(day_num) + "/" + String(hour) + "h";
      } else if (selected_period == 1 && i > 0 && i % 3 == 0) {
        // 12h: afficher jour tous les 3 points
        history_api_labels[i] = String(day_num) + "/" + String(hour) + "h";
      } else {
        history_api_labels[i] = String(hour) + "h";
      }
      
      history_api_ext[i] = temp;
      
      float preds[NUM_ROOMS];
      predict_rooms(temp, 50.0, day_num, month_num, preds);  // humidity par défaut 50%
      
      // Ajouter variation horaire pour rendre les courbes plus dynamiques
      float hour_factor = sin((float)hour / 24.0f * 6.28f) * 0.5f; // Variation ±0.5°C selon l'heure
      
      for (int r = 0; r < NUM_ROOMS; r++) {
        history_api_rooms[i][r] = preds[r] + hour_factor * (0.3f + r * 0.15f);  // Variation progressive
      }
      
      history_api_count++;
    }
  } else {
    // Mode journalier
    if (!doc.containsKey("daily") || !doc["daily"].containsKey("temperature_2m_mean")) {
      Serial.println("[SECURITY] Structure JSON daily invalide");
      return;
    }
    
    JsonArray temps = doc["daily"]["temperature_2m_mean"];
    JsonArray dates = doc["daily"]["time"];
    
    for (int i = 0; i < API_HISTORY_SIZE && i < temps.size(); i++) {
      float temp = temps[i];
      
      // Validation température
      if (!validate_temperature(temp)) {
        Serial.printf("[SECURITY] Température #%d ignorée\n", i);
        continue;
      }
      
      String date_str = dates[i].as<String>();
      if (date_str.length() < 10) continue;
      
      // Extraire date: format "2025-11-26"
      int day_num = date_str.substring(8, 10).toInt();
      int month_num = date_str.substring(5, 7).toInt();
      history_api_labels[i] = String(day_num) + "/" + String(month_num);
      
      history_api_ext[i] = temp;
      
      float preds[NUM_ROOMS];
      predict_rooms(temp, 50.0, day_num, month_num, preds);  // humidity par défaut 50%
      
      // Ajouter variation journalière pour rendre les courbes plus dynamiques
      float day_factor = sin((float)day_num / 31.0f * 6.28f) * 0.8f; // Variation ±0.8°C selon le jour
      
      for (int r = 0; r < NUM_ROOMS; r++) {
        history_api_rooms[i][r] = preds[r] + day_factor * (0.4f + r * 0.2f);  // Variation progressive
      }
      
      history_api_count++;
    }
  }
  
  Serial.printf("[SUCCESS] %d prévisions récupérées\n", history_api_count);
}

void add_to_manual_history(float ext, float r1, float r2, float r4) {
  String label = manual_time.substring(0, 2) + "h";  // Ex: "14h"
  
  if (history_manual_count < HISTORY_SIZE) {
    history_manual_ext[history_manual_count] = ext;
    history_manual_r1[history_manual_count] = r1;
    history_manual_r2[history_manual_count] = r2;
    history_manual_r4[history_manual_count] = r4;
    history_manual_labels[history_manual_count] = label;
    history_manual_count++;
  } else {
    // Décaler l'historique
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
      history_manual_ext[i] = history_manual_ext[i + 1];
      history_manual_r1[i] = history_manual_r1[i + 1];
      history_manual_r2[i] = history_manual_r2[i + 1];
      history_manual_r4[i] = history_manual_r4[i + 1];
      history_manual_labels[i] = history_manual_labels[i + 1];
    }
    history_manual_ext[HISTORY_SIZE - 1] = ext;
    history_manual_r1[HISTORY_SIZE - 1] = r1;
    history_manual_r2[HISTORY_SIZE - 1] = r2;
    history_manual_r4[HISTORY_SIZE - 1] = r4;
    history_manual_labels[HISTORY_SIZE - 1] = label;
  }
}

void reset_manual_history() {
  history_manual_count = 0;
  for (int i = 0; i < HISTORY_SIZE; i++) {
    history_manual_ext[i] = 0;
    history_manual_r1[i] = 0;
    history_manual_r2[i] = 0;
    history_manual_r4[i] = 0;
    history_manual_labels[i] = "";
  }
}

// ============================================================================
// NEURAL NETWORK
// ============================================================================

// Calculer jour de l'année (1-365) depuis jour/mois
int day_of_year(int day, int month) {
  // Jours cumulés par mois (année non bissextile)
  const int days_in_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  
  if (month < 1 || month > 12) return 1;
  if (day < 1 || day > 31) return 1;
  
  return days_in_month[month - 1] + day;
}

// Encodage cyclique de la date (sin/cos pour continuité)
void encode_date_cyclical(int day, int month, float* sin_day, float* cos_day) {
  int doy = day_of_year(day, month);  // 1-365
  float angle = (2.0 * PI * doy) / 365.0;  // Radians
  
  *sin_day = sin(angle);
  *cos_day = cos(angle);
  
  Serial.printf("[DATE] Jour de l'année: %d/365 → sin=%.3f, cos=%.3f\n", doy, *sin_day, *cos_day);
}

float relu(float x) {
  return x > 0 ? x : 0.0f;
}

void predict_rooms(float temp_ext, float humidity, int day, int month, float* output) {
  unsigned long start_time = micros(); // Mesure précise en microsecondes
  
  // Encoder la saison (jour de l'année) de manière cyclique
  float season_sin, season_cos;
  encode_date_cyclical(day, month, &season_sin, &season_cos);
  
  // Encoder l'heure du jour (pour simplification, utiliser une valeur fixe ou heure actuelle)
  // Pour l'instant, utiliser midi (12h) comme valeur par défaut
  float time_sin = sin(2.0 * PI * 12.0 / 24.0);
  
  // INPUT: 5 features (temp_ext, humidity, season_sin, season_cos, time_sin)
  float inputs[5] = {temp_ext, humidity, season_sin, season_cos, time_sin};
  
  Serial.printf("[NN] Inputs: temp=%.1f°C, hum=%.1f%%, season_sin=%.3f, season_cos=%.3f, time_sin=%.3f\n", 
                temp_ext, humidity, season_sin, season_cos, time_sin);
  
  // Layer 0: Input(5) -> Dense(32) + ReLU
  float hidden1[32];
  for (int i = 0; i < 32; i++) {
    hidden1[i] = BIAS0[i];
    for (int j = 0; j < 5; j++) {  // 5 inputs
      hidden1[i] += inputs[j] * W0[j][i];
    }
    hidden1[i] = relu(hidden1[i]);
  }
  
  // Layer 1: Dense(32) -> Dense(32) + ReLU
  float hidden2[32];
  for (int i = 0; i < 32; i++) {
    hidden2[i] = BIAS1[i];
    for (int j = 0; j < 32; j++) {
      hidden2[i] += hidden1[j] * W1[j][i];
    }
    hidden2[i] = relu(hidden2[i]);
  }
  
  // Layer 2: Dense(32) -> Output(NUM_ROOMS)
  for (int i = 0; i < NUM_ROOMS; i++) {
    output[i] = BIAS2[i];
    for (int j = 0; j < 32; j++) {
      output[i] += hidden2[j] * W2[j][i];
    }
  }
  
  // Enregistrer temps d'inférence
  perf.last_inference_time = micros() - start_time;
  Serial.printf("[PERF] Temps inférence NN: %lu µs (%.3f ms)\n", 
                perf.last_inference_time, 
                perf.last_inference_time / 1000.0);
}

// ============================================================================
// DISPLAY
// ============================================================================

// Afficher statistiques de performance (debugging/démonstration)
void display_performance_stats() {
  Serial.println("\n======== PERFORMANCE STATISTICS ========");
  Serial.printf("RAM libre actuelle: %lu bytes\n", perf.total_free_heap);
  Serial.printf("RAM libre minimum: %lu bytes\n", perf.min_free_heap);
  Serial.printf("Dernière latence API: %lu ms\n", perf.last_api_latency);
  Serial.printf("Dernier temps inférence NN: %lu µs (%.3f ms)\n", 
                perf.last_inference_time, 
                perf.last_inference_time / 1000.0);
  Serial.printf("APIs réussies: %d\n", perf.api_success_count);
  Serial.printf("APIs échouées: %d\n", perf.api_failure_count);
  
  if (perf.api_success_count + perf.api_failure_count > 0) {
    float success_rate = (100.0 * perf.api_success_count) / 
                         (perf.api_success_count + perf.api_failure_count);
    Serial.printf("Taux de réussite API: %.1f%%\n", success_rate);
  }
  
  Serial.printf("Requêtes en queue: %d/%d\n", queue_count, MAX_QUEUE_SIZE);
  Serial.println("========================================\n");
}

void display_menu() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé bleu foncé
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 0, 40 + y/20);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec effet ombre (1 ligne)
  M5.Display.setTextSize(5);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 120));
  M5.Display.setCursor(82, 22);
  M5.Display.println("ROOM TEMPERATURE PREDICTION");
  
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(80, 20);
  M5.Display.println("ROOM TEMPERATURE PREDICTION");
  
  // Statut WiFi avec icône
  M5.Display.setTextSize(3);
  if (wifi_connected) {
    M5.Display.fillRoundRect(950, 10, 310, 40, 20, M5.Display.color565(0, 100, 0));
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(970, 18);
    M5.Display.print("WiFi");
    M5.Display.fillCircle(1220, 30, 12, GREEN);
  } else {
    M5.Display.fillRoundRect(950, 10, 310, 40, 20, M5.Display.color565(100, 0, 0));
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(970, 18);
    M5.Display.print("WiFi");
    M5.Display.fillCircle(1220, 30, 12, RED);
  }
  
  // Option 1: Prédiction manuelle (avec dégradé et ombre)
  M5.Display.fillRoundRect(40, 165, 1200, 105, 15, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(40, 165, 1200, 105, 15, M5.Display.color565(100, 150, 255));
  M5.Display.drawRoundRect(41, 166, 1198, 103, 15, M5.Display.color565(100, 150, 255));
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(6);
  M5.Display.setCursor(240, 195);  // Centré: (1200 - 720) / 2 + 40 = 280, ajusté visuellement
  M5.Display.println("PREDICTION MANUELLE");
  
  // Option 2: Température réelle
  M5.Display.fillRoundRect(40, 285, 1200, 105, 15, M5.Display.color565(0, 150, 0));
  M5.Display.drawRoundRect(40, 285, 1200, 105, 15, M5.Display.color565(100, 255, 100));
  M5.Display.drawRoundRect(41, 286, 1198, 103, 15, M5.Display.color565(100, 255, 100));
  M5.Display.setCursor(250, 315);  // Centré
  M5.Display.println("TEMP. REELLE (WiFi)");
  
  // Option 3: Graphique Manuel
  M5.Display.fillRoundRect(40, 405, 385, 105, 15, M5.Display.color565(200, 100, 0));
  M5.Display.drawRoundRect(40, 405, 385, 105, 15, M5.Display.color565(255, 180, 100));
  M5.Display.drawRoundRect(41, 406, 383, 103, 15, M5.Display.color565(255, 180, 100));
  M5.Display.setTextSize(4);
  M5.Display.setCursor(85, 440);  // Centré: (385 - 288) / 2 + 40 = 88
  M5.Display.println("GRAPH MANUEL");
  
  // Option 4: Graphique API
  M5.Display.fillRoundRect(447, 405, 385, 105, 15, M5.Display.color565(0, 150, 200));
  M5.Display.drawRoundRect(447, 405, 385, 105, 15, M5.Display.color565(100, 200, 255));
  M5.Display.drawRoundRect(448, 406, 383, 103, 15, M5.Display.color565(100, 200, 255));
  M5.Display.setCursor(530, 440);  // Centré: (385 - 240) / 2 + 447 = 520
  M5.Display.println("PREDICTION TEMP");
  
  // Option 5: Données passées (NOUVEAU)
  M5.Display.fillRoundRect(855, 405, 385, 105, 15, M5.Display.color565(150, 0, 150));
  M5.Display.drawRoundRect(855, 405, 385, 105, 15, M5.Display.color565(255, 100, 255));
  M5.Display.drawRoundRect(856, 406, 383, 103, 15, M5.Display.color565(255, 100, 255));
  M5.Display.setCursor(905, 440);  // Centré: (385 - 288) / 2 + 855 = 903
  M5.Display.println("DONNEES CSV");
  
  // Option 6: WiFi Config
  M5.Display.fillRoundRect(40, 525, 1200, 180, 15, M5.Display.color565(80, 80, 80));
  M5.Display.drawRoundRect(40, 525, 1200, 180, 15, M5.Display.color565(150, 150, 150));
  M5.Display.drawRoundRect(41, 526, 1198, 178, 15, M5.Display.color565(150, 150, 150));
  M5.Display.setTextSize(6);
  M5.Display.setCursor(270, 590);  // Centré: (1200 - 660) / 2 + 40 = 310, ajusté
  M5.Display.println("CONFIGURATION WiFi");
  
  M5.Display.endWrite();  // Fin du buffering
}

// ===== FONCTIONS DE MISE À JOUR PARTIELLE =====
// Mise à jour UNIQUEMENT de la température affichée (sans redessiner tout l'écran)
void update_temperature_display() {
  M5.Display.startWrite();
  
  // Effacer zone température (rectangle principal)
  M5.Display.fillRoundRect(100, 140, 1080, 180, 20, M5.Display.color565(30, 30, 60));
  M5.Display.drawRoundRect(100, 140, 1080, 180, 20, CYAN);
  M5.Display.drawRoundRect(101, 141, 1078, 178, 20, M5.Display.color565(100, 200, 255));
  M5.Display.drawRoundRect(102, 142, 1076, 176, 20, M5.Display.color565(0, 150, 200));
  
  // Réafficher température
  M5.Display.setTextSize(12);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(420, 180);
  M5.Display.printf("%.1f", current_temp);
  M5.Display.setTextSize(9);
  M5.Display.setCursor(840, 190);
  M5.Display.println("C");
  
  // Info increment
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(100, 200, 255));
  M5.Display.setCursor(540, 275);
  M5.Display.println("(+/- 0.1 C)");
  
  M5.Display.endWrite();
}

// Mise à jour UNIQUEMENT de la date/heure affichée
void update_datetime_display() {
  M5.Display.startWrite();
  
  // Zone jour (même position que display_input)
  M5.Display.fillRoundRect(200, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(200, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(208, 50);
  M5.Display.printf("%02d", manual_day);
  
  // Zone mois (même position que display_input)
  M5.Display.fillRoundRect(390, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(390, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(398, 50);
  M5.Display.printf("%02d", manual_month);
  
  // Zone heure (même position que display_input)
  M5.Display.fillRoundRect(700, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(700, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(708, 50);
  M5.Display.printf("%02d", manual_hour);
  
  // Zone minute (même position que display_input)
  M5.Display.fillRoundRect(890, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(890, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(898, 50);
  M5.Display.printf("%02d", manual_minute);
  
  M5.Display.endWrite();
}

// Mise à jour UNIQUEMENT de la zone de saisie du clavier
void update_keyboard_input_display() {
  M5.Display.startWrite();
  
  // Effacer zone de saisie
  M5.Display.fillRoundRect(8, 60, 1264, 88, 12, M5.Display.color565(30, 30, 50));
  M5.Display.drawRoundRect(8, 60, 1264, 88, 12, CYAN);
  M5.Display.drawRoundRect(9, 61, 1262, 86, 12, M5.Display.color565(100, 150, 200));
  
  // Afficher mot de passe masqué
  M5.Display.setTextColor(YELLOW);
  M5.Display.setTextSize(4);
  M5.Display.setCursor(20, 88);
  
  String masked = "";
  for (int i = 0; i < keyboard_input.length(); i++) {
    masked += "*";
  }
  if (masked.length() > 70) {
    masked = masked.substring(masked.length() - 70);
  }
  M5.Display.println(masked);
  
  M5.Display.endWrite();
}

// Mise à jour UNIQUEMENT des touches du clavier (changement de mode ABC/123/MAJ)
void update_keyboard_keys() {
  M5.Display.startWrite();
  
  // Déterminer quel jeu de touches afficher
  const char* keys_lower[] = {"azertyuiop", "qsdfghjklm", "wxcvbn,;.!"};
  const char* keys_upper[] = {"AZERTYUIOP", "QSDFGHJKLM", "WXCVBN,;.!"};
  const char* keys_num[] = {"1234567890", "@#$%&*()-+", "[]{}:;\"'<>"};
  
  const char** current_keys = (keyboard_mode == 2) ? keys_num : 
                               (keyboard_mode == 1) ? keys_upper : keys_lower;
  
  int key_w = 124;
  int key_h = 96;
  int start_y = 180;
  
  // Redessiner les 30 touches principales (3 lignes × 10 colonnes)
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 10; col++) {
      int x = col * 128;
      int y = start_y + row * 102;
      
      // Touches avec effet 3D
      M5.Display.fillRoundRect(x, y, key_w, key_h, 8, M5.Display.color565(60, 60, 80));
      M5.Display.drawRoundRect(x, y, key_w, key_h, 8, M5.Display.color565(100, 100, 150));
      M5.Display.drawRoundRect(x+1, y+1, key_w-2, key_h-2, 8, M5.Display.color565(80, 80, 120));
      
      M5.Display.setTextColor(WHITE);
      M5.Display.setTextSize(6);
      M5.Display.setCursor(x + 40, y + 27);
      M5.Display.print(current_keys[row][col]);
    }
  }
  
  // Redessiner boutons spéciaux (MODE et SHIFT changent d'apparence)
  int special_y = start_y + 3 * 102;
  
  // MODE (ABC/123)
  M5.Display.fillRoundRect(0, special_y, 256, key_h, 8, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(0, special_y, 256, key_h, 8, BLUE);
  M5.Display.drawRoundRect(1, special_y+1, 254, key_h-2, 8, M5.Display.color565(100, 150, 255));
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(4);
  M5.Display.setCursor(72, special_y + 36);
  M5.Display.println(keyboard_mode == 2 ? "ABC" : "123");
  
  // SHIFT (Maj) - Couleur change selon état
  uint16_t shift_color = keyboard_mode == 1 ? M5.Display.color565(200, 100, 0) : M5.Display.color565(60, 60, 80);
  uint16_t shift_border = keyboard_mode == 1 ? ORANGE : M5.Display.color565(100, 100, 150);
  M5.Display.fillRoundRect(264, special_y, 248, key_h, 8, shift_color);
  M5.Display.drawRoundRect(264, special_y, 248, key_h, 8, shift_border);
  M5.Display.drawRoundRect(265, special_y+1, 246, key_h-2, 8, shift_border);
  M5.Display.setCursor(320, special_y + 36);
  M5.Display.println("MAJ");
  
  M5.Display.endWrite();
}

void display_input() {
  M5.Display.startWrite();  // Début du buffering pour éviter la cassure visuelle
  
  // Fond dégradé bleu-violet
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(10, 10 + y/25, 40 + y/15);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(M5.Display.color565(0, 60, 100));
  M5.Display.setCursor(322, 7);
  M5.Display.println("PREDICTION MANUELLE");
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(320, 5);
  M5.Display.println("PREDICTION MANUELLE");
  
  // Panel date/heure avec fond arrondi
  M5.Display.fillRoundRect(20, 35, 1020, 50, 10, M5.Display.color565(30, 30, 70));
  M5.Display.drawRoundRect(20, 35, 1020, 50, 10, M5.Display.color565(100, 100, 200));
  
  // DATE
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(30, 50);
  M5.Display.print("Date:");
  
  // Boutons - jour (style moderne)
  M5.Display.fillRoundRect(150, 45, 45, 30, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(150, 45, 45, 30, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(162, 48);
  M5.Display.print("-");
  
  // Affichage jour avec style card
  M5.Display.fillRoundRect(200, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(200, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(208, 50);
  M5.Display.printf("%02d", manual_day);
  
  // Boutons + jour
  M5.Display.fillRoundRect(265, 45, 45, 30, 8, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(265, 45, 45, 30, 8, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(277, 48);
  M5.Display.print("+");
  
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(315, 50);
  M5.Display.print("/");
  
  // Boutons - mois
  M5.Display.fillRoundRect(340, 45, 45, 30, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(340, 45, 45, 30, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(4);
  M5.Display.setCursor(352, 48);
  M5.Display.print("-");
  
  // Affichage mois
  M5.Display.fillRoundRect(390, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(390, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(398, 50);
  M5.Display.printf("%02d", manual_month);
  
  // Boutons + mois
  M5.Display.fillRoundRect(455, 45, 45, 30, 8, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(455, 45, 45, 30, 8, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(467, 48);
  M5.Display.print("+");
  
  // HEURE
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(560, 50);
  M5.Display.print("Heure:");
  
  // Boutons - heure
  M5.Display.fillRoundRect(650, 45, 45, 30, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(650, 45, 45, 30, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(4);
  M5.Display.setCursor(662, 48);
  M5.Display.print("-");
  
  // Affichage heure
  M5.Display.fillRoundRect(700, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(700, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(708, 50);
  M5.Display.printf("%02d", manual_hour);
  
  // Boutons + heure
  M5.Display.fillRoundRect(765, 45, 45, 30, 8, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(765, 45, 45, 30, 8, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(777, 48);
  M5.Display.print("+");
  
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(815, 50);
  M5.Display.print(":");
  
  // Boutons - minute
  M5.Display.fillRoundRect(840, 45, 45, 30, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(840, 45, 45, 30, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(4);
  M5.Display.setCursor(852, 48);
  M5.Display.print("-");
  
  // Affichage minute
  M5.Display.fillRoundRect(890, 45, 60, 30, 8, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(890, 45, 60, 30, 8, YELLOW);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(898, 50);
  M5.Display.printf("%02d", manual_minute);
  
  // Boutons + minute
  M5.Display.fillRoundRect(955, 45, 45, 30, 8, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(955, 45, 45, 30, 8, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(967, 48);
  M5.Display.print("+");
  
  // Bouton AUTO (style moderne avec violet)
  M5.Display.fillRoundRect(1050, 40, 210, 40, 10, M5.Display.color565(100, 0, 150));
  M5.Display.drawRoundRect(1050, 40, 210, 40, 10, M5.Display.color565(180, 100, 255));
  M5.Display.drawRoundRect(1051, 41, 208, 38, 10, M5.Display.color565(180, 100, 255));
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(1070, 50);
  M5.Display.print("AUTO WiFi");
  
  // Panel température avec effet 3D
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(380, 100);
  M5.Display.println("Temperature Exterieure:");
  
  // Affichage température avec style moderne
  M5.Display.fillRoundRect(100, 140, 1080, 180, 20, M5.Display.color565(30, 30, 60));
  M5.Display.drawRoundRect(100, 140, 1080, 180, 20, CYAN);
  M5.Display.drawRoundRect(101, 141, 1078, 178, 20, M5.Display.color565(100, 200, 255));
  M5.Display.drawRoundRect(102, 142, 1076, 176, 20, M5.Display.color565(0, 150, 200));
  
  M5.Display.setTextSize(12);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(420, 180);
  M5.Display.printf("%.1f", current_temp);
  M5.Display.setTextSize(9);
  M5.Display.setCursor(840, 190);
  M5.Display.println("C");
  
  // Info increment avec style
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(100, 200, 255));
  M5.Display.setCursor(540, 275);
  M5.Display.println("(+/- 0.1 C)");
  
  // Boutons température avec style 3D
  M5.Display.fillRoundRect(50, 340, 580, 55, 15, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(50, 340, 580, 55, 15, RED);
  M5.Display.drawRoundRect(51, 341, 578, 53, 15, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(292, 350);
  M5.Display.println("-");
  
  M5.Display.fillRoundRect(650, 340, 580, 55, 15, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(650, 340, 580, 55, 15, GREEN);
  M5.Display.drawRoundRect(651, 341, 578, 53, 15, M5.Display.color565(100, 255, 100));
  M5.Display.setCursor(892, 350);
  M5.Display.println("+");
  
  // Boutons BACK et PREDICT avec style moderne
  M5.Display.fillRoundRect(50, 410, 580, 70, 15, M5.Display.color565(200, 100, 0));
  M5.Display.drawRoundRect(50, 410, 580, 70, 15, ORANGE);
  M5.Display.drawRoundRect(51, 411, 578, 68, 15, M5.Display.color565(255, 180, 100));
  M5.Display.setTextSize(6);
  M5.Display.setCursor(220, 430);
  M5.Display.println("BACK");
  
  M5.Display.fillRoundRect(650, 410, 580, 70, 15, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(650, 410, 580, 70, 15, BLUE);
  M5.Display.drawRoundRect(651, 411, 578, 68, 15, M5.Display.color565(100, 150, 255));
  M5.Display.setTextSize(6);
  M5.Display.setCursor(760, 430);
  M5.Display.println("PREDICT");
  
  M5.Display.endWrite();  // Fin du buffering - affichage instantané
}

void display_result() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé vert-bleu
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 20 + y/30, 30 + y/20);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Header avec dégradé et ombre
  M5.Display.fillRoundRect(20, 10, 1240, 90, 15, M5.Display.color565(40, 40, 70));
  M5.Display.drawRoundRect(20, 10, 1240, 90, 15, M5.Display.color565(100, 100, 200));
  M5.Display.drawRoundRect(21, 11, 1238, 88, 15, M5.Display.color565(100, 100, 200));
  
  M5.Display.setTextSize(8);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 0));
  M5.Display.setCursor(182, 20);
  M5.Display.printf("Externe: %.1f C", current_temp);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(180, 18);
  M5.Display.printf("Externe: %.1f C", current_temp);
  
  // Cartes pour chaque chambre avec effet 3D
  int card_y = 120;
  int card_h = 150;
  
  // ROOM 1 (Vert)
  M5.Display.fillRoundRect(40, card_y, 1200, card_h, 20, M5.Display.color565(20, 60, 20));
  M5.Display.drawRoundRect(40, card_y, 1200, card_h, 20, GREEN);
  M5.Display.drawRoundRect(41, card_y+1, 1198, card_h-2, 20, M5.Display.color565(100, 255, 100));
  M5.Display.drawRoundRect(42, card_y+2, 1196, card_h-4, 20, M5.Display.color565(0, 200, 0));
  
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(80, card_y + 20);
  M5.Display.println("ROOM 1");
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(GREEN);
  M5.Display.setCursor(600, card_y + 40);
  M5.Display.printf("%.1f C", predictions[0]);
  M5.Display.setTextSize(5);
  M5.Display.setTextColor(M5.Display.color565(100, 255, 255));
  M5.Display.setCursor(80, card_y + 100);
  M5.Display.printf("(+%.1f C)", predictions[0] - current_temp);
  
  // ROOM 2 (Bleu)
  card_y = 290;
  M5.Display.fillRoundRect(40, card_y, 580, card_h, 20, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(40, card_y, 580, card_h, 20, BLUE);
  M5.Display.drawRoundRect(41, card_y+1, 578, card_h-2, 20, M5.Display.color565(100, 100, 255));
  M5.Display.drawRoundRect(42, card_y+2, 576, card_h-4, 20, M5.Display.color565(0, 0, 200));
  
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(60, card_y + 20);
  M5.Display.println("ROOM 2");
  M5.Display.setTextSize(8);
  M5.Display.setTextColor(BLUE);
  M5.Display.setCursor(120, card_y + 60);
  M5.Display.printf("%.1f C", predictions[1]);
  
  // ROOM 3 (Jaune)
  M5.Display.fillRoundRect(660, card_y, 580, card_h, 20, M5.Display.color565(60, 60, 20));
  M5.Display.drawRoundRect(660, card_y, 580, card_h, 20, YELLOW);
  M5.Display.drawRoundRect(661, card_y+1, 578, card_h-2, 20, M5.Display.color565(255, 255, 100));
  M5.Display.drawRoundRect(662, card_y+2, 576, card_h-4, 20, M5.Display.color565(200, 200, 0));
  
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(680, card_y + 20);
  M5.Display.println("ROOM 3");
  M5.Display.setTextSize(8);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(740, card_y + 60);
  M5.Display.printf("%.1f C", predictions[2]);
  
  // Différences moyennes avec style
  M5.Display.fillRoundRect(300, 455, 680, 50, 10, M5.Display.color565(30, 30, 50));
  M5.Display.drawRoundRect(300, 455, 680, 50, 10, CYAN);
  
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(320, 468);
  float avg_diff = (predictions[0] + predictions[1] + predictions[2]) / 3.0 - current_temp;
  M5.Display.printf("Difference moyenne: +%.1f C", avg_diff);
  
  // Boutons avec style moderne
  M5.Display.fillRoundRect(50, 615, 580, 105, 15, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(50, 615, 580, 105, 15, BLUE);
  M5.Display.drawRoundRect(51, 616, 578, 103, 15, M5.Display.color565(100, 150, 255));
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(200, 640);
  M5.Display.println("GRAPH");
  
  M5.Display.fillRoundRect(650, 615, 580, 105, 15, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(650, 615, 580, 105, 15, RED);
  M5.Display.drawRoundRect(651, 616, 578, 103, 15, M5.Display.color565(255, 100, 100));
  M5.Display.setCursor(820, 640);
  M5.Display.println("MENU");
  
  M5.Display.endWrite();  // Fin du buffering
}

void display_graph_manual() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé violet
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(10 + y/40, 0, 30 + y/25);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  if (history_manual_count < 2) {
    M5.Display.setTextSize(6);
    M5.Display.setTextColor(CYAN);
    M5.Display.setCursor(370, 6);  // Centré: (1280 - 540) / 2 ≈ 370
    M5.Display.println("HISTO MANUEL");
    
    M5.Display.setTextSize(8);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(360, 300);  // Centré: (1280 - 560) / 2 = 360
    M5.Display.println("Pas assez");
    M5.Display.setCursor(220, 360);  // Centré: (1280 - 840) / 2 = 220
    M5.Display.println("de donnees");
    
    // Bouton BACK arrondi centré
    M5.Display.fillRoundRect(390, 550, 500, 100, 15, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(390, 550, 500, 100, 15, RED);
    M5.Display.drawRoundRect(391, 551, 498, 98, 15, M5.Display.color565(255, 100, 100));
    M5.Display.setTextSize(7);  // Réduit de 9 à 7
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(560, 575);  // Centré: (500 - 168) / 2 + 390 ≈ 560
    M5.Display.println("BACK");
    
    M5.Display.endWrite();  // Fin du buffering
    return;
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 100));
  M5.Display.setCursor(22, 12);
  M5.Display.println("HISTO MANUEL");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(20, 10);
  M5.Display.println("HISTO MANUEL");
  
  // Date et heure en haut à droite
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(750, 15);
  if (wifi_connected) {
    HTTPClient http;
    http.begin("http://worldtimeapi.org/api/timezone/Europe/Paris");
    int httpCode = http.GET();
    if (httpCode == 200) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, http.getString());
      String datetime = doc["datetime"].as<String>();
      String date = datetime.substring(8, 10) + "/" + datetime.substring(5, 7);
      String time = datetime.substring(11, 16);
      M5.Display.printf("%s %s", date.c_str(), time.c_str());
    }
    http.end();
  } else {
    M5.Display.print("--/-- --:--");
  }
  
  // Menu déroulant pour sélection des chambres (haut droite)
  int dropdown_x = 880, dropdown_y = 10, dropdown_w = 380, dropdown_h = 50;
  M5.Display.fillRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, M5.Display.color565(20, 20, 80));
  M5.Display.drawRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, CYAN);
  
  // Flèche GAUCHE
  int arrow_size = 15;
  M5.Display.fillTriangle(
    dropdown_x + 20, dropdown_y + dropdown_h/2,
    dropdown_x + 20 + arrow_size, dropdown_y + dropdown_h/2 - arrow_size/2,
    dropdown_x + 20 + arrow_size, dropdown_y + dropdown_h/2 + arrow_size/2,
    WHITE
  );
  
  // Flèche DROITE
  M5.Display.fillTriangle(
    dropdown_x + dropdown_w - 20, dropdown_y + dropdown_h/2,
    dropdown_x + dropdown_w - 20 - arrow_size, dropdown_y + dropdown_h/2 - arrow_size/2,
    dropdown_x + dropdown_w - 20 - arrow_size, dropdown_y + dropdown_h/2 + arrow_size/2,
    WHITE
  );
  
  // Texte centré
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(YELLOW);
  String label = getRoomViewLabel(selected_room_view);
  int text_w = label.length() * 24;
  M5.Display.setCursor(dropdown_x + (dropdown_w - text_w)/2, dropdown_y + 12);
  M5.Display.print(label);
  
  // Légende détaillée à gauche (uniquement si "TOUTES" sélectionné)
  if (selected_room_view == 0) {
    M5.Display.fillRoundRect(10, 80, 80, 140, 8, M5.Display.color565(20, 20, 50));
    M5.Display.drawRoundRect(10, 80, 80, 140, 8, M5.Display.color565(100, 100, 150));
    
    M5.Display.setTextSize(2);
    M5.Display.fillCircle(23, 100, 4, RED);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(33, 95);
    M5.Display.print("Ext");
    
    M5.Display.fillCircle(23, 130, 4, GREEN);
    M5.Display.setCursor(33, 125);
    M5.Display.print("R1");
    
    M5.Display.fillCircle(23, 160, 4, BLUE);
    M5.Display.setCursor(33, 155);
    M5.Display.print("R2");
    
    M5.Display.fillCircle(23, 190, 4, YELLOW);
    M5.Display.setCursor(33, 185);
    M5.Display.print("R4");
  }
  
  // Zone graphique avec bordure arrondie
  int graph_x = 180;
  int graph_y = 70;
  int graph_w = 1020;
  int graph_h = 550;
  
  M5.Display.drawRoundRect(graph_x, graph_y, graph_w, graph_h, 5, WHITE);
  M5.Display.drawRoundRect(graph_x+1, graph_y+1, graph_w-2, graph_h-2, 5, M5.Display.color565(150, 150, 150));
  
  // Trouver min/max pour échelle adaptative
  float min_temp = history_manual_ext[0];
  float max_temp = history_manual_ext[0];
  
  for (int i = 0; i < history_manual_count; i++) {
    // Toujours vérifier température extérieure
    if (history_manual_ext[i] < min_temp) min_temp = history_manual_ext[i];
    if (history_manual_ext[i] > max_temp) max_temp = history_manual_ext[i];
    
    // Vérifier seulement les rooms visibles selon le filtre
    if (selected_room_view == 0 || selected_room_view == 1) {
      if (history_manual_r1[i] < min_temp) min_temp = history_manual_r1[i];
      if (history_manual_r1[i] > max_temp) max_temp = history_manual_r1[i];
    }
    if (selected_room_view == 0 || selected_room_view == 2) {
      if (history_manual_r2[i] < min_temp) min_temp = history_manual_r2[i];
      if (history_manual_r2[i] > max_temp) max_temp = history_manual_r2[i];
    }
    if (selected_room_view == 0 || selected_room_view == 3) {
      if (history_manual_r4[i] < min_temp) min_temp = history_manual_r4[i];
      if (history_manual_r4[i] > max_temp) max_temp = history_manual_r4[i];
    }
  }
  
  // Arrondir l'échelle intelligemment
  float temp_range = max_temp - min_temp;
  if (temp_range < 1) temp_range = 1;
  
  // Ajouter marge 20% pour éviter que courbes touchent les bords
  min_temp -= temp_range * 0.2f;
  max_temp += temp_range * 0.2f;
  temp_range = max_temp - min_temp;
  
  // Graduations verticales avec affichage décimal
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(M5.Display.color565(100, 100, 100));
  for (int i = 0; i <= 4; i++) {
    float temp_val = min_temp + (temp_range * i / 4);
    int y_pos = graph_y + graph_h - (i * graph_h / 4);
    
    M5.Display.drawLine(graph_x, y_pos, graph_x + graph_w, y_pos, M5.Display.color565(50, 50, 50));
    
    M5.Display.setCursor(100, y_pos - 12);
    M5.Display.printf("%.1f", temp_val);
  }
  
  // Dessiner les courbes (5 pixels d'épaisseur pour rendu plus lisse) + points
  for (int i = 1; i < history_manual_count; i++) {
    int x1 = graph_x + (i - 1) * graph_w / (HISTORY_SIZE - 1);
    int x2 = graph_x + i * graph_w / (HISTORY_SIZE - 1);
    
    // Ext (Rouge)
    int y1_ext = graph_y + graph_h - (history_manual_ext[i-1] - min_temp) / temp_range * graph_h;
    int y2_ext = graph_y + graph_h - (history_manual_ext[i] - min_temp) / temp_range * graph_h;
    for(int t=-2; t<=2; t++) {
      M5.Display.drawLine(x1, y1_ext+t, x2, y2_ext+t, RED);
    }
    M5.Display.fillCircle(x2, y2_ext, 6, RED);
    M5.Display.fillCircle(x2, y2_ext, 3, WHITE);
    
    // R1 (Vert) - Afficher si "TOUTES" ou "ROOM 1"
    if (selected_room_view == 0 || selected_room_view == 1) {
      int y1_r1 = graph_y + graph_h - (history_manual_r1[i-1] - min_temp) / temp_range * graph_h;
      int y2_r1 = graph_y + graph_h - (history_manual_r1[i] - min_temp) / temp_range * graph_h;
      for(int t=-2; t<=2; t++) {
        M5.Display.drawLine(x1, y1_r1+t, x2, y2_r1+t, GREEN);
      }
      M5.Display.fillCircle(x2, y2_r1, 6, GREEN);
      M5.Display.fillCircle(x2, y2_r1, 3, BLACK);
    }
    
    // R2 (Bleu) - Afficher si "TOUTES" ou "ROOM 2"
    if (selected_room_view == 0 || selected_room_view == 2) {
      int y1_r2 = graph_y + graph_h - (history_manual_r2[i-1] - min_temp) / temp_range * graph_h;
      int y2_r2 = graph_y + graph_h - (history_manual_r2[i] - min_temp) / temp_range * graph_h;
      for(int t=-2; t<=2; t++) {
        M5.Display.drawLine(x1, y1_r2+t, x2, y2_r2+t, BLUE);
      }
      M5.Display.fillCircle(x2, y2_r2, 6, BLUE);
      M5.Display.fillCircle(x2, y2_r2, 3, WHITE);
    }
    
    // R4 (Jaune) - Afficher si "TOUTES" ou "ROOM 3"
    if (selected_room_view == 0 || selected_room_view == 3) {
      int y1_r4 = graph_y + graph_h - (history_manual_r4[i-1] - min_temp) / temp_range * graph_h;
      int y2_r4 = graph_y + graph_h - (history_manual_r4[i] - min_temp) / temp_range * graph_h;
      for(int t=-2; t<=2; t++) {
        M5.Display.drawLine(x1, y1_r4+t, x2, y2_r4+t, YELLOW);
      }
      M5.Display.fillCircle(x2, y2_r4, 6, YELLOW);
      M5.Display.fillCircle(x2, y2_r4, 3, BLACK);
    }
  }
  
  // Labels horizontaux (heures)
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  int label_y = graph_y + graph_h + 8;
  
  for (int i = 0; i < history_manual_count; i++) {
    int x_pos = graph_x + i * graph_w / (HISTORY_SIZE - 1);
    
    if (history_manual_labels[i].length() > 0) {
      M5.Display.setCursor(x_pos - 15, label_y);
      M5.Display.print(history_manual_labels[i]);
    }
  }
  
  // Info en bas avec fond
  M5.Display.fillRoundRect(158, 628, 1100, 28, 8, M5.Display.color565(20, 20, 40));
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(168, 635);
  M5.Display.printf("%d predictions | Min: %.1fC | Max: %.1fC | Date: %s", 
                    history_manual_count, min_temp, max_temp, manual_date.c_str());
  
  // Boutons arrondis
  M5.Display.fillRoundRect(50, 660, 560, 60, 15, M5.Display.color565(200, 100, 0));
  M5.Display.drawRoundRect(50, 660, 560, 60, 15, ORANGE);
  M5.Display.drawRoundRect(51, 661, 558, 58, 15, M5.Display.color565(255, 180, 100));
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(130, 665);
  M5.Display.println("RESET");
  
  M5.Display.fillRoundRect(670, 660, 560, 60, 15, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(670, 660, 560, 60, 15, RED);
  M5.Display.drawRoundRect(671, 661, 558, 58, 15, M5.Display.color565(255, 100, 100));
  M5.Display.setCursor(800, 665);
  M5.Display.println("BACK");
  
  M5.Display.endWrite();  // Fin du buffering
}

// ============================================================================
// AFFICHAGE DES DONNÉES HISTORIQUES CSV
// ============================================================================
void display_historical_data() {
  M5.Display.startWrite();
  
  // Fond dégradé violet
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(20 + y/40, 0, 30 + y/25);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(280, 8);
  M5.Display.printf("DONNEES CSV - %s", csv_period_labels[csv_period]);
  
  // Menu déroulant période en haut droite
  int dropdown_x = 880, dropdown_y = 10, dropdown_w = 380, dropdown_h = 50;
  M5.Display.fillRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, M5.Display.color565(40, 0, 60));
  M5.Display.drawRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, MAGENTA);
  
  // Flèche GAUCHE
  int arrow_size = 15;
  M5.Display.fillTriangle(
    dropdown_x + 20, dropdown_y + dropdown_h/2,
    dropdown_x + 20 + arrow_size, dropdown_y + dropdown_h/2 - arrow_size/2,
    dropdown_x + 20 + arrow_size, dropdown_y + dropdown_h/2 + arrow_size/2,
    WHITE
  );
  
  // Flèche DROITE
  M5.Display.fillTriangle(
    dropdown_x + dropdown_w - 20, dropdown_y + dropdown_h/2,
    dropdown_x + dropdown_w - 20 - arrow_size, dropdown_y + dropdown_h/2 - arrow_size/2,
    dropdown_x + dropdown_w - 20 - arrow_size, dropdown_y + dropdown_h/2 + arrow_size/2,
    WHITE
  );
  
  // Texte centré
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(YELLOW);
  int text_w = strlen(csv_period_labels[csv_period]) * 20;
  M5.Display.setCursor(dropdown_x + (dropdown_w - text_w)/2, dropdown_y + 12);
  M5.Display.print(csv_period_labels[csv_period]);
  
  // Menu déroulant sélection chambre (même style que Prediction Temp)
  int room_dd_x = 50, room_dd_y = 10, room_dd_w = 300, room_dd_h = 50;
  M5.Display.fillRoundRect(room_dd_x, room_dd_y, room_dd_w, room_dd_h, 10, M5.Display.color565(20, 20, 80));
  M5.Display.drawRoundRect(room_dd_x, room_dd_y, room_dd_w, room_dd_h, 10, CYAN);
  
  // Flèches pour room
  M5.Display.fillTriangle(
    room_dd_x + 15, room_dd_y + room_dd_h/2,
    room_dd_x + 15 + arrow_size, room_dd_y + room_dd_h/2 - arrow_size/2,
    room_dd_x + 15 + arrow_size, room_dd_y + room_dd_h/2 + arrow_size/2,
    WHITE
  );
  M5.Display.fillTriangle(
    room_dd_x + room_dd_w - 15, room_dd_y + room_dd_h/2,
    room_dd_x + room_dd_w - 15 - arrow_size, room_dd_y + room_dd_h/2 - arrow_size/2,
    room_dd_x + room_dd_w - 15 - arrow_size, room_dd_y + room_dd_h/2 + arrow_size/2,
    WHITE
  );
  
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  String room_label = getRoomViewLabel(selected_room_view);
  int room_text_w = room_label.length() * 18;
  M5.Display.setCursor(room_dd_x + (room_dd_w - room_text_w)/2, room_dd_y + 15);
  M5.Display.print(room_label);
  
  // Le nombre de points sera déterminé dynamiquement depuis sizes_array
  
  // Zone graphique (décalée pour laisser place aux graduations)
  int graph_x = 180;
  int graph_y = 70;
  int graph_w = 1020;
  int graph_h = 490;
  
  M5.Display.drawRoundRect(graph_x, graph_y, graph_w, graph_h, 5, WHITE);
  M5.Display.drawRoundRect(graph_x+1, graph_y+1, graph_w-2, graph_h-2, 5, M5.Display.color565(150, 150, 150));
  
  // Calculer min/max réels depuis les VRAIES données CSV
  float min_temp = 100.0f;
  float max_temp = -100.0f;
  
  // Accès via la nouvelle structure linéaire
  const float* temps_array = csv_temps_arrays[csv_period];
  const int* offsets_array = csv_offsets_arrays[csv_period];
  const int* sizes_array = csv_sizes_arrays[csv_period];
  
  // Calculer min/max en fonction des rooms visibles
  for (int room_idx = 0; room_idx < CSV_NUM_ROOMS; room_idx++) {
    // Si "Toutes" OU si c'est la room sélectionnée
    if (selected_room_view == 0 || selected_room_view == room_idx + 1) {
      int offset = pgm_read_word(&offsets_array[room_idx]);
      int size = pgm_read_word(&sizes_array[room_idx]);
      
      for (int i = 0; i < size; i++) {
        float temp = pgm_read_float(&temps_array[offset + i]);
        if (temp < min_temp) min_temp = temp;
        if (temp > max_temp) max_temp = temp;
      }
    }
  }
  
  // Ajouter marge 20% pour éviter que courbes touchent les bords
  float temp_range = max_temp - min_temp;
  min_temp -= temp_range * 0.2f;
  max_temp += temp_range * 0.2f;
  temp_range = max_temp - min_temp;
  
  // Graduations verticales
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  for (int i = 0; i <= 4; i++) {
    float temp_val = min_temp + (temp_range * i / 4);
    int y_pos = graph_y + graph_h - (i * graph_h / 4);
    
    M5.Display.drawLine(graph_x, y_pos, graph_x + graph_w, y_pos, M5.Display.color565(50, 50, 80));
    
    M5.Display.setCursor(100, y_pos - 8);
    M5.Display.printf("%.1f", temp_val);
  }
  
  // Graduations horizontales (dates)
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  
  // Calculer intervalle selon période
  int time_labels = 5;  // 5 labels sur l'axe X
  for (int i = 0; i <= time_labels; i++) {
    int x_pos = graph_x + (i * graph_w / time_labels);
    M5.Display.drawLine(x_pos, graph_y, x_pos, graph_y + graph_h, M5.Display.color565(50, 50, 80));
    
    // Calculer le label temporel
    int hours_ago = csv_samples[csv_period] / 2 * (time_labels - i) / time_labels;
    
    M5.Display.setCursor(x_pos - 30, graph_y + graph_h + 10);
    if (csv_period == 0) {  // 1 jour
      M5.Display.printf("-%dh", hours_ago);
    } else if (csv_period == 1) {  // 1 semaine
      int days = hours_ago / 24;
      M5.Display.printf("-%dj", days);
    } else if (csv_period == 2) {  // 1 mois
      int days = hours_ago / 24;
      M5.Display.printf("-%dj", days);
    } else {  // 3 mois
      int days = hours_ago / 24;
      M5.Display.printf("-%dj", days);
    }
  }
  
  // Dessiner courbes RÉELLES depuis les données CSV (nouveau format scalable)
  // Couleurs pour chaque room (cycle sur 10 couleurs)
  uint16_t room_colors[10] = {GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, WHITE, RED, M5.Display.color565(255, 192, 203), M5.Display.color565(128, 255, 0)};
  
  // Dessiner chaque room visible
  for (int room_idx = 0; room_idx < CSV_NUM_ROOMS; room_idx++) {
    // Si "Toutes" OU si c'est la room sélectionnée
    if (selected_room_view == 0 || selected_room_view == room_idx + 1) {
      int offset = pgm_read_word(&offsets_array[room_idx]);
      int size = pgm_read_word(&sizes_array[room_idx]);
      uint16_t color = room_colors[room_idx % 10];
      
      // Dessiner la courbe point par point
      for (int i = 1; i < size; i++) {
        float temp_prev = pgm_read_float(&temps_array[offset + i - 1]);
        float temp_curr = pgm_read_float(&temps_array[offset + i]);
        
        int x1 = graph_x + (i - 1) * graph_w / (size - 1);
        int x2 = graph_x + i * graph_w / (size - 1);
        int y1 = graph_y + graph_h - (temp_prev - min_temp) / temp_range * graph_h;
        int y2 = graph_y + graph_h - (temp_curr - min_temp) / temp_range * graph_h;
        
        // Ligne épaisse
        for(int t=-2; t<=2; t++) {
          M5.Display.drawLine(x1, y1+t, x2, y2+t, color);
        }
        M5.Display.fillCircle(x2, y2, 4, color);
      }
    }
  }
  
  // Légende si "TOUTES" (adaptative pour éviter débordement)
  if (selected_room_view == 0) {
    // Calculer espacement dynamique pour tenir dans l'écran
    int available_height = 520;  // De y=80 à y=600
    int spacing = (CSV_NUM_ROOMS > 12) ? 20 : 30;  // Réduire espacement si >12 rooms
    int legend_h = 40 + CSV_NUM_ROOMS * spacing;
    if (legend_h > available_height) {
      legend_h = available_height;
      spacing = (available_height - 40) / CSV_NUM_ROOMS;
    }
    
    M5.Display.fillRoundRect(10, 80, 80, legend_h, 8, M5.Display.color565(20, 0, 30));
    M5.Display.drawRoundRect(10, 80, 80, legend_h, 8, M5.Display.color565(150, 0, 150));
    
    M5.Display.setTextSize(2);
    for (int room_idx = 0; room_idx < CSV_NUM_ROOMS; room_idx++) {
      int y_pos = 95 + room_idx * spacing;
      M5.Display.fillCircle(23, y_pos, 4, room_colors[room_idx % 10]);
      M5.Display.setTextColor(WHITE);
      M5.Display.setCursor(33, y_pos - 5);
      M5.Display.printf("R%d", room_idx + 1);
    }
  }
  
  // Info en bas - afficher nom de la room sélectionnée
  M5.Display.fillRoundRect(150, 570, 950, 30, 8, M5.Display.color565(30, 0, 50));
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(160, 575);
  
  if (selected_room_view == 0) {
    M5.Display.printf("Donnees CSV - TOUTES | %d rooms | Min: %.1fC | Max: %.1fC", 
                      CSV_NUM_ROOMS, min_temp, max_temp);
  } else {
    String room_label = getRoomViewLabel(selected_room_view);
    int room_idx = selected_room_view - 1;
    int num_points = pgm_read_word(&sizes_array[room_idx]);
    M5.Display.printf("%s | %d points | Min: %.1fC | Max: %.1fC", 
                      room_label.c_str(), num_points, min_temp, max_temp);
  }
  
  // Bouton BACK
  M5.Display.fillRoundRect(390, 620, 500, 80, 15, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(390, 620, 500, 80, 15, RED);
  M5.Display.drawRoundRect(391, 621, 498, 78, 15, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(550, 640);
  M5.Display.println("BACK");
  
  M5.Display.endWrite();
}

void display_graph_api() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé bleu-cyan
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 10 + y/35, 40 + y/18);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  if (!wifi_connected) {
    M5.Display.setTextSize(6);
    M5.Display.setTextColor(CYAN);
    M5.Display.setCursor(200, 30);
    M5.Display.printf("PREVISIONS %s", period_labels[selected_period]);
    
    M5.Display.setTextSize(8);
    M5.Display.setTextColor(RED);
    M5.Display.setCursor(340, 250);
    M5.Display.println("WiFi non");
    M5.Display.setCursor(280, 340);
    M5.Display.println("connecte");
    
    // Bouton BACK moderne centré
    M5.Display.fillRoundRect(320, 550, 640, 120, 20, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(320, 550, 640, 120, 20, RED);
    M5.Display.drawRoundRect(321, 551, 638, 118, 20, M5.Display.color565(255, 100, 100));
    M5.Display.setTextSize(9);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(520, 580);
    M5.Display.println("BACK");
    
    M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant return
    return;
  }
  
  // Afficher "Chargement..."
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(200, 6);
  M5.Display.printf("PREVISIONS %s", period_labels[selected_period]);
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(280, 300);
  M5.Display.println("Chargement");
  M5.Display.setCursor(260, 360);
  M5.Display.println("previsions...");
  
  M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant fetch API
  M5.Display.display();   // Forcer l'affichage
  
  // Récupérer prévisions API
  fetch_api_forecast();
  
  M5.Display.startWrite();  // ✅ FIX: Redémarrer buffering après fetch
  
  // Redessiner fond
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 10 + y/35, 40 + y/18);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  if (history_api_count < 2) {
    M5.Display.setTextSize(6);
    M5.Display.setTextColor(CYAN);
    M5.Display.setCursor(200, 30);
    M5.Display.printf("PREVISIONS %s", period_labels[selected_period]);
    
    M5.Display.setTextSize(7);
    M5.Display.setTextColor(RED);
    M5.Display.setCursor(370, 200);
    M5.Display.println("Erreur API");
    
    M5.Display.setTextSize(4);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setCursor(220, 310);
    M5.Display.println("Patientez 10 secondes");
    M5.Display.setCursor(180, 360);
    M5.Display.println("avant nouvelle requete");
    
    // Bouton BACK moderne centré
    M5.Display.fillRoundRect(320, 550, 640, 120, 20, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(320, 550, 640, 120, 20, RED);
    M5.Display.drawRoundRect(321, 551, 638, 118, 20, M5.Display.color565(255, 100, 100));
    M5.Display.setTextSize(9);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(520, 580);
    M5.Display.println("BACK");
    
    M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant return
    return;
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 120));
  M5.Display.setCursor(72, 12);
  M5.Display.printf("PREVISIONS %s", period_labels[selected_period]);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(70, 10);
  M5.Display.printf("PREVISIONS %s", period_labels[selected_period]);
  
  // Menu déroulant de sélection de chambre (haut droite)
  int dropdown_x = 880;
  int dropdown_y = 10;
  int dropdown_w = 380;
  int dropdown_h = 50;
  
  // Fond du dropdown
  M5.Display.fillRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, M5.Display.color565(30, 30, 60));
  M5.Display.drawRoundRect(dropdown_x, dropdown_y, dropdown_w, dropdown_h, 10, CYAN);
  
  // Flèches gauche/droite
  M5.Display.fillTriangle(dropdown_x + 15, dropdown_y + 25,
                          dropdown_x + 5, dropdown_y + 15,
                          dropdown_x + 5, dropdown_y + 35, WHITE);
  
  M5.Display.fillTriangle(dropdown_x + dropdown_w - 15, dropdown_y + 25,
                          dropdown_x + dropdown_w - 5, dropdown_y + 15,
                          dropdown_x + dropdown_w - 5, dropdown_y + 35, WHITE);
  
  // Texte de la sélection actuelle
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(YELLOW);
  String room_label = getRoomViewLabel(selected_room_view);
  int text_w = room_label.length() * 24;
  M5.Display.setCursor(dropdown_x + (dropdown_w - text_w) / 2, dropdown_y + 14);
  M5.Display.print(room_label);
  
  // Légende des couleurs (seulement si "TOUTES" est sélectionné)
  if (selected_room_view == 0) {
    M5.Display.fillRoundRect(10, 75, 140, 160, 10, M5.Display.color565(20, 20, 50));
    M5.Display.drawRoundRect(10, 75, 140, 160, 10, M5.Display.color565(100, 100, 150));
    
    M5.Display.setTextSize(3);
    
    // Ext
    M5.Display.fillCircle(30, 95, 8, RED);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(45, 87);
    M5.Display.print("Ext");
    
    // R1
    M5.Display.fillCircle(30, 130, 8, GREEN);
    M5.Display.setCursor(45, 122);
    M5.Display.print("R1");
    
    // R2
    M5.Display.fillCircle(30, 165, 8, BLUE);
    M5.Display.setCursor(45, 157);
    M5.Display.print("R2");
    
    // R3
    M5.Display.fillCircle(30, 200, 8, YELLOW);
    M5.Display.setCursor(45, 192);
    M5.Display.print("R3");
  }
  
  // Zone graphique avec bordure arrondie
  int graph_x = 180;
  int graph_y = 70;
  int graph_w = 1020;
  int graph_h = 475;
  
  M5.Display.drawRoundRect(graph_x, graph_y, graph_w, graph_h, 5, WHITE);
  M5.Display.drawRoundRect(graph_x+1, graph_y+1, graph_w-2, graph_h-2, 5, M5.Display.color565(150, 150, 150));
  
  // Trouver min/max pour échelle adaptative
  float min_temp = history_api_ext[0];
  float max_temp = history_api_ext[0];
  
  for (int i = 0; i < history_api_count; i++) {
    // Toujours vérifier température extérieure
    if (history_api_ext[i] < min_temp) min_temp = history_api_ext[i];
    if (history_api_ext[i] > max_temp) max_temp = history_api_ext[i];
    
    // Vérifier seulement les rooms visibles selon le filtre
    for (int room_idx = 0; room_idx < NUM_ROOMS; room_idx++) {
      if (selected_room_view == 0 || selected_room_view == room_idx + 1) {
        if (history_api_rooms[i][room_idx] < min_temp) min_temp = history_api_rooms[i][room_idx];
        if (history_api_rooms[i][room_idx] > max_temp) max_temp = history_api_rooms[i][room_idx];
      }
    }
  }
  
  // Arrondir l'échelle intelligemment
  float temp_range = max_temp - min_temp;
  if (temp_range < 1) temp_range = 1;
  
  // Ajouter marge 20% pour éviter que courbes touchent les bords
  min_temp -= temp_range * 0.2f;
  max_temp += temp_range * 0.2f;
  temp_range = max_temp - min_temp;
  
  // Graduations verticales avec affichage décimal
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(M5.Display.color565(100, 100, 100));
  for (int i = 0; i <= 4; i++) {
    float temp_val = min_temp + (temp_range * i / 4);
    int y_pos = graph_y + graph_h - (i * graph_h / 4);
    
    M5.Display.drawLine(graph_x, y_pos, graph_x + graph_w, y_pos, M5.Display.color565(50, 50, 50));
    
    M5.Display.setCursor(100, y_pos - 12);
    M5.Display.printf("%.1f", temp_val);
  }
  
  // Dessiner les courbes (5 pixels d'épaisseur) + points
  for (int i = 1; i < history_api_count; i++) {
    int x1 = graph_x + (i - 1) * graph_w / (history_api_count - 1);
    int x2 = graph_x + i * graph_w / (history_api_count - 1);
    
    // Ext (Rouge) - Toujours affiché
    int y1_ext = graph_y + graph_h - (history_api_ext[i-1] - min_temp) / temp_range * graph_h;
    int y2_ext = graph_y + graph_h - (history_api_ext[i] - min_temp) / temp_range * graph_h;
    for(int t=-2; t<=2; t++) {
      M5.Display.drawLine(x1, y1_ext+t, x2, y2_ext+t, RED);
    }
    M5.Display.fillCircle(x2, y2_ext, 6, RED);
    M5.Display.fillCircle(x2, y2_ext, 3, WHITE);
    
    // Dessiner courbes des rooms (dynamique selon NUM_ROOMS)
    uint16_t room_colors[10] = {GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, WHITE, M5.Display.color565(255, 192, 203), M5.Display.color565(128, 255, 0), M5.Display.color565(255, 128, 0)};
    
    for (int room_idx = 0; room_idx < NUM_ROOMS; room_idx++) {
      // Afficher si "TOUTES" ou si c'est la room sélectionnée
      if (selected_room_view == 0 || selected_room_view == room_idx + 1) {
        int y1 = graph_y + graph_h - (history_api_rooms[i-1][room_idx] - min_temp) / temp_range * graph_h;
        int y2 = graph_y + graph_h - (history_api_rooms[i][room_idx] - min_temp) / temp_range * graph_h;
        uint16_t color = room_colors[room_idx % 10];
        
        for(int t=-2; t<=2; t++) {
          M5.Display.drawLine(x1, y1+t, x2, y2+t, color);
        }
        M5.Display.fillCircle(x2, y2, 6, color);
        M5.Display.fillCircle(x2, y2, 3, (room_idx % 2 == 0) ? BLACK : WHITE);
      }
    }
  }
  
  // Labels horizontaux
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  int label_y = graph_y + graph_h + 8;
  
  for (int i = 0; i < history_api_count; i++) {
    int x_pos = graph_x + i * graph_w / (history_api_count - 1);
    
    if (history_api_count <= 8 || i % 2 == 0) {
      M5.Display.setCursor(x_pos - 18, label_y);
      M5.Display.print(history_api_labels[i]);
    }
  }
  
  // Info en bas avec fond (déplacée pour ne pas masquer les dates)
  M5.Display.fillRoundRect(158, 595, 1100, 28, 8, M5.Display.color565(20, 20, 40));
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(168, 602);
  M5.Display.printf("%d points | Min: %.1fC | Max: %.1fC | Periode: %s", 
                    history_api_count, min_temp, max_temp, period_labels[selected_period]);
  
  // Bouton BACK moderne (réduit pour laisser voir les dates)
  M5.Display.fillRoundRect(390, 635, 500, 80, 20, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(390, 635, 500, 80, 20, RED);
  M5.Display.drawRoundRect(391, 636, 498, 78, 20, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(515, 655);
  M5.Display.println("BACK");
  
  M5.Display.endWrite();  // Fin du buffering
}

void display_real_temp() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé orange-jaune
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(20 + y/30, 15 + y/40, 0);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(M5.Display.color565(150, 100, 0));
  M5.Display.setCursor(282, 22);
  M5.Display.println("TEMP REELLE (WiFi)");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(280, 20);
  M5.Display.println("TEMP REELLE (WiFi)");
  
  if (!wifi_connected) {
    M5.Display.setTextSize(8);
    M5.Display.setTextColor(RED);
    M5.Display.setCursor(360, 250);  // Centré: (1280 - 560) / 2 = 360
    M5.Display.println("WiFi non");
    M5.Display.setCursor(280, 340);  // Centré: (1280 - 720) / 2 = 280
    M5.Display.println("connecte");
    
    M5.Display.setTextSize(5);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setCursor(280, 440);  // Centré: (1280 - 700) / 2 ≈ 290
    M5.Display.println("Utilisez WiFi Config");
    
    // Bouton BACK moderne
    M5.Display.fillRoundRect(320, 550, 640, 120, 20, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(320, 550, 640, 120, 20, RED);
    M5.Display.drawRoundRect(321, 551, 638, 118, 20, M5.Display.color565(255, 100, 100));
    M5.Display.setTextSize(9);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(520, 580);
    M5.Display.println("BACK");
    
    M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant return
    return;
  }
  
  // Message de chargement
  M5.Display.setTextSize(8);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(240, 300);
  M5.Display.println("Recuperation...");
  
  M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant API call
  M5.Display.display();   // Forcer l'affichage
  
  real_temp = get_real_temperature();
  
  M5.Display.startWrite();  // ✅ FIX: Redémarrer buffering après API call
  
  // Redessiner fond
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(20 + y/30, 15 + y/40, 0);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(M5.Display.color565(150, 100, 0));
  M5.Display.setCursor(282, 22);
  M5.Display.println("TEMP REELLE (WiFi)");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(280, 20);
  M5.Display.println("TEMP REELLE (WiFi)");
  
  if (real_temp < -100) {
    M5.Display.setTextSize(7);
    M5.Display.setTextColor(RED);
    M5.Display.setCursor(370, 200);
    M5.Display.println("Erreur API");
    
    M5.Display.setTextSize(4);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setCursor(220, 310);
    M5.Display.println("Patientez 10 secondes");
    M5.Display.setCursor(180, 360);
    M5.Display.println("avant nouvelle requete");
    
    // Bouton BACK moderne
    M5.Display.fillRoundRect(320, 550, 640, 120, 20, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(320, 550, 640, 120, 20, RED);
    M5.Display.drawRoundRect(321, 551, 638, 118, 20, M5.Display.color565(255, 100, 100));
    M5.Display.setTextSize(9);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(520, 580);
    M5.Display.println("BACK");
    
    M5.Display.endWrite();  // ✅ FIX: Terminer buffering avant return
    return;
  }
  
  // Prédiction automatique avec date actuelle
  current_temp = real_temp;
  predict_rooms(current_temp, 50.0, manual_day, manual_month, predictions);  // humidity par défaut 50%
  add_to_manual_history(current_temp, predictions[0], predictions[1], predictions[2]);
  
  // Panneau température extérieure avec style
  M5.Display.fillRoundRect(150, 110, 980, 90, 20, M5.Display.color565(60, 50, 0));
  M5.Display.drawRoundRect(150, 110, 980, 90, 20, YELLOW);
  M5.Display.drawRoundRect(151, 111, 978, 88, 20, M5.Display.color565(255, 220, 100));
  M5.Display.drawRoundRect(152, 112, 976, 86, 20, M5.Display.color565(200, 180, 0));
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(330, 130);
  M5.Display.printf("Ext: %.1f C", current_temp);
  
  // ROOM 1 avec carte moderne
  M5.Display.fillRoundRect(60, 225, 1160, 80, 15, M5.Display.color565(20, 60, 20));
  M5.Display.drawRoundRect(60, 225, 1160, 80, 15, GREEN);
  M5.Display.drawRoundRect(61, 226, 1158, 78, 15, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(9);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(80, 240);
  M5.Display.println("ROOM 1:");
  M5.Display.setTextColor(GREEN);
  M5.Display.setCursor(700, 240);
  M5.Display.printf("%.1f C", predictions[0]);
  
  // ROOM 2 avec carte moderne
  M5.Display.fillRoundRect(60, 330, 1160, 80, 15, M5.Display.color565(20, 20, 60));
  M5.Display.drawRoundRect(60, 330, 1160, 80, 15, BLUE);
  M5.Display.drawRoundRect(61, 331, 1158, 78, 15, M5.Display.color565(100, 100, 255));
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(80, 345);
  M5.Display.println("ROOM 2:");
  M5.Display.setTextColor(BLUE);
  M5.Display.setCursor(700, 345);
  M5.Display.printf("%.1f C", predictions[1]);
  
  // ROOM 3 avec carte moderne
  M5.Display.fillRoundRect(60, 435, 1160, 80, 15, M5.Display.color565(60, 60, 20));
  M5.Display.drawRoundRect(60, 435, 1160, 80, 15, YELLOW);
  M5.Display.drawRoundRect(61, 436, 1158, 78, 15, M5.Display.color565(255, 255, 100));
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(80, 450);
  M5.Display.println("ROOM 3:");
  M5.Display.setTextColor(YELLOW);
  M5.Display.setCursor(700, 450);
  M5.Display.printf("%.1f C", predictions[2]);
  
  // Boutons modernes
  M5.Display.fillRoundRect(50, 615, 580, 105, 15, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(50, 615, 580, 105, 15, BLUE);
  M5.Display.drawRoundRect(51, 616, 578, 103, 15, M5.Display.color565(100, 150, 255));
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(180, 640);
  M5.Display.println("GRAPH");
  
  M5.Display.fillRoundRect(650, 615, 580, 105, 15, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(650, 615, 580, 105, 15, RED);
  M5.Display.drawRoundRect(651, 616, 578, 103, 15, M5.Display.color565(255, 100, 100));
  M5.Display.setCursor(840, 640);
  M5.Display.println("MENU");
  
  M5.Display.endWrite();  // Fin du buffering
}

// ✅ NOUVELLE PAGE : Gestion des réseaux WiFi sauvegardés
void display_wifi_config() {
  M5.Display.startWrite();  // Début du buffering
  
  // ✅ Si c'est le premier affichage (pas de scan fait), faire un scan automatique
  static bool first_display = true;
  if (first_display && scanned_count == 0) {
    first_display = false;
    M5.Display.endWrite();  // Fermer le buffer avant le scan
    scan_wifi_networks();
    M5.Display.startWrite();  // Rouvrir le buffer
  }
  
  // Fond dégradé bleu-violet
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 10 + y/40, 50 + y/20);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 140));
  M5.Display.setCursor(282, 8);
  M5.Display.println("CONFIG WiFi");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(280, 6);
  M5.Display.println("CONFIG WiFi");
  
  if (scanned_count == 0) {
    M5.Display.setTextSize(6);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(200, 300);
    M5.Display.println("Aucun reseau");
    M5.Display.setCursor(360, 375);
    M5.Display.println("trouve");
    
    // Boutons arrondis
    M5.Display.fillRoundRect(40, 540, 580, 150, 20, M5.Display.color565(0, 80, 200));
    M5.Display.drawRoundRect(40, 540, 580, 150, 20, BLUE);
    M5.Display.drawRoundRect(41, 541, 578, 148, 20, M5.Display.color565(100, 150, 255));
    M5.Display.setTextSize(6);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(112, 585);
    M5.Display.println("SCANNER");
    
    M5.Display.fillRoundRect(660, 540, 580, 150, 20, M5.Display.color565(150, 0, 0));
    M5.Display.drawRoundRect(660, 540, 580, 150, 20, RED);
    M5.Display.drawRoundRect(661, 541, 578, 148, 20, M5.Display.color565(255, 100, 100));
    M5.Display.setCursor(792, 585);
    M5.Display.println("BACK");
    return;
  }
  
  // Afficher liste des réseaux (max 6 visibles avec défilement)
  M5.Display.setTextSize(6);
  int max_visible = 6;
  int display_count = min(scanned_count - wifi_scroll_offset, max_visible);
  
  for (int i = 0; i < display_count; i++) {
    int network_index = i + wifi_scroll_offset;
    int y_pos = 75 + (i * 102);
    
    // Fond sélection arrondi
    if (network_index == selected_network) {
      M5.Display.fillRoundRect(20, y_pos - 6, 1240, 96, 12, M5.Display.color565(60, 60, 100));
      M5.Display.drawRoundRect(20, y_pos - 6, 1240, 96, 12, YELLOW);
    } else {
      M5.Display.fillRoundRect(20, y_pos - 6, 1240, 96, 12, M5.Display.color565(30, 30, 60));
      M5.Display.drawRoundRect(20, y_pos - 6, 1240, 96, 12, M5.Display.color565(80, 80, 120));
    }
    
    // Force du signal (barres)
    int bars = 0;
    if (scanned_rssi[network_index] > -50) bars = 4;
    else if (scanned_rssi[network_index] > -60) bars = 3;
    else if (scanned_rssi[network_index] > -70) bars = 2;
    else bars = 1;
    
    for (int b = 0; b < bars; b++) {
      M5.Display.fillRect(40 + b*18, y_pos + 66 - b*15, 15, 15 + b*15, GREEN);
    }
    
    // SSID
    M5.Display.setTextColor(network_index == selected_network ? YELLOW : WHITE);
    M5.Display.setCursor(160, y_pos + 24);
    String ssid_display = scanned_ssids[network_index];
    if (ssid_display.length() > 40) {
      ssid_display = ssid_display.substring(0, 40) + "...";
    }
    M5.Display.println(ssid_display);
  }
  
  // Indicateur de défilement (flèches haut/bas)
  if (wifi_scroll_offset > 0) {
    M5.Display.fillTriangle(1220, 90, 1200, 110, 1240, 110, YELLOW);
  }
  if (wifi_scroll_offset + max_visible < scanned_count) {
    M5.Display.fillTriangle(1220, 600, 1200, 580, 1240, 580, YELLOW);
  }
  
  // Compteur réseaux avec fond
  M5.Display.fillRoundRect(1040, 5, 230, 40, 10, M5.Display.color565(30, 30, 60));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(1055, 15);
  M5.Display.printf("%d/%d", wifi_scroll_offset + 1, scanned_count);
  
  // ✅ Boutons en bas (3 boutons : RESCAN, CONNECT, BACK)
  M5.Display.setTextSize(5);
  
  // RESCAN
  M5.Display.fillRoundRect(20, 630, 290, 75, 12, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(20, 630, 290, 75, 12, BLUE);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(50, 650);
  M5.Display.println("RESCAN");
  
  // CONNECT (seulement si réseau sélectionné)
  if (selected_network >= 0) {
    M5.Display.fillRoundRect(330, 630, 290, 75, 12, M5.Display.color565(0, 120, 0));
    M5.Display.drawRoundRect(330, 630, 290, 75, 12, GREEN);
    M5.Display.setCursor(350, 650);
    M5.Display.println("CONNECT");
  }
  
  // BACK
  M5.Display.fillRoundRect(640, 630, 310, 75, 12, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(640, 630, 310, 75, 12, RED);
  M5.Display.setCursor(720, 650);
  M5.Display.println("BACK");
  
  M5.Display.endWrite();  // Fin du buffering
}

void display_period_select() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé vert-cyan
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(0, 20 + y/30, 30 + y/25);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 100));
  M5.Display.setCursor(182, 32);
  M5.Display.println("SELECTIONNER PERIODE");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(180, 30);
  M5.Display.println("SELECTIONNER PERIODE");
  
  // Instructions
  M5.Display.setTextSize(5);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(140, 120);
  M5.Display.println("Choisissez la duree des previsions:");
  
  // Grille de 4 boutons (2 colonnes × 2 lignes)
  int btn_w = 580;
  int btn_h = 180;
  int margin_x = 60;
  int margin_y = 220;
  int spacing_x = 640;
  int spacing_y = 200;
  
  const uint16_t colors_base[] = {
    M5.Display.color565(0, 100, 150),
    M5.Display.color565(0, 0, 150),
    M5.Display.color565(0, 120, 0),
    M5.Display.color565(150, 100, 0)
  };
  const uint16_t colors_border[] = {
    CYAN,
    M5.Display.color565(100, 100, 255),
    GREEN,
    YELLOW
  };
  
  for (int i = 0; i < 4; i++) {
    int row = i / 2;
    int col = i % 2;
    
    int x = margin_x + col * spacing_x;
    int y = margin_y + row * spacing_y;
    
    // Bouton avec effet lumineux si sélectionné
    if (i == selected_period) {
      // Triple bordure pour effet lumineux
      M5.Display.fillRoundRect(x, y, btn_w, btn_h, 20, WHITE);
      M5.Display.fillRoundRect(x+6, y+6, btn_w-12, btn_h-12, 17, colors_border[i]);
      M5.Display.fillRoundRect(x+12, y+12, btn_w-24, btn_h-24, 14, colors_base[i]);
    } else {
      M5.Display.fillRoundRect(x, y, btn_w, btn_h, 20, colors_base[i]);
      M5.Display.drawRoundRect(x, y, btn_w, btn_h, 20, colors_border[i]);
      M5.Display.drawRoundRect(x+1, y+1, btn_w-2, btn_h-2, 20, colors_border[i]);
    }
    
    // Texte
    M5.Display.setTextSize(7);  // Réduit de 9 à 7 pour que le texte rentre
    M5.Display.setTextColor(WHITE);
    
    // Centrer le texte
    int text_x = x + btn_w/2 - strlen(period_labels[i]) * 21;
    int text_y = y + btn_h/2 - 21;
    
    M5.Display.setCursor(text_x, text_y);
    M5.Display.println(period_labels[i]);
  }
  
  // Bouton RETOUR moderne
  M5.Display.fillRoundRect(320, 650, 640, 70, 20, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(320, 650, 640, 70, 20, RED);
  M5.Display.drawRoundRect(321, 651, 638, 68, 20, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(7);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(480, 668);
  M5.Display.println("RETOUR");
  
  M5.Display.endWrite();  // Fin du buffering
}

void display_keyboard() {
  M5.Display.startWrite();  // Début du buffering
  
  // Fond dégradé gris-bleu foncé
  for (int y = 0; y < 720; y++) {
    uint16_t color = M5.Display.color565(10 + y/50, 10 + y/50, 20 + y/35);
    M5.Display.drawFastHLine(0, y, 1280, color);
  }
  
  // Titre avec ombre
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(M5.Display.color565(0, 80, 100));
  M5.Display.setCursor(282, 10);
  M5.Display.println("SAISIR MOT DE PASSE");
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(280, 8);
  M5.Display.println("SAISIR MOT DE PASSE");
  
  // Zone de saisie moderne
  M5.Display.fillRoundRect(8, 60, 1264, 88, 12, M5.Display.color565(30, 30, 50));
  M5.Display.drawRoundRect(8, 60, 1264, 88, 12, CYAN);
  M5.Display.drawRoundRect(9, 61, 1262, 86, 12, M5.Display.color565(100, 150, 200));
  M5.Display.setTextColor(YELLOW);
  M5.Display.setTextSize(4);
  M5.Display.setCursor(20, 88);
  
  // Afficher mot de passe masqué
  String masked = "";
  for (int i = 0; i < keyboard_input.length(); i++) {
    masked += "*";
  }
  if (masked.length() > 70) {
    masked = masked.substring(masked.length() - 70);
  }
  M5.Display.println(masked);
  
  // Clavier (3 lignes de 10 touches chacune)
  const char* keys_lower[] = {
    "azertyuiop",
    "qsdfghjklm",
    "wxcvbn,;.!"
  };
  const char* keys_upper[] = {
    "AZERTYUIOP",
    "QSDFGHJKLM",
    "WXCVBN,;.!"
  };
  const char* keys_num[] = {
    "1234567890",
    "@#$%&*()-+",
    "[]{}:;\"'<>"
  };
  
  const char** current_keys = (keyboard_mode == 2) ? keys_num : 
                               (keyboard_mode == 1) ? keys_upper : keys_lower;
  
  int key_w = 124;
  int key_h = 96;
  int start_y = 180;
  
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 10; col++) {
      int x = col * 128;
      int y = start_y + row * 102;
      
      // Touches avec effet 3D
      M5.Display.fillRoundRect(x, y, key_w, key_h, 8, M5.Display.color565(60, 60, 80));
      M5.Display.drawRoundRect(x, y, key_w, key_h, 8, M5.Display.color565(100, 100, 150));
      M5.Display.drawRoundRect(x+1, y+1, key_w-2, key_h-2, 8, M5.Display.color565(80, 80, 120));
      
      M5.Display.setTextColor(WHITE);
      M5.Display.setTextSize(6);
      M5.Display.setCursor(x + 40, y + 27);
      M5.Display.print(current_keys[row][col]);
    }
  }
  
  // Ligne de touches spéciales
  int special_y = start_y + 3 * 102;
  
  // MODE (ABC/123)
  M5.Display.fillRoundRect(0, special_y, 256, key_h, 8, M5.Display.color565(0, 80, 200));
  M5.Display.drawRoundRect(0, special_y, 256, key_h, 8, BLUE);
  M5.Display.drawRoundRect(1, special_y+1, 254, key_h-2, 8, M5.Display.color565(100, 150, 255));
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(4);
  M5.Display.setCursor(72, special_y + 36);
  M5.Display.println(keyboard_mode == 2 ? "ABC" : "123");
  
  // SHIFT (Maj)
  uint16_t shift_color = keyboard_mode == 1 ? M5.Display.color565(200, 100, 0) : M5.Display.color565(60, 60, 80);
  uint16_t shift_border = keyboard_mode == 1 ? ORANGE : M5.Display.color565(100, 100, 150);
  M5.Display.fillRoundRect(264, special_y, 248, key_h, 8, shift_color);
  M5.Display.drawRoundRect(264, special_y, 248, key_h, 8, shift_border);
  M5.Display.drawRoundRect(265, special_y+1, 246, key_h-2, 8, shift_border);
  M5.Display.setCursor(320, special_y + 36);
  M5.Display.println("MAJ");
  
  // ESPACE
  M5.Display.fillRoundRect(520, special_y, 240, key_h, 8, M5.Display.color565(60, 60, 80));
  M5.Display.drawRoundRect(520, special_y, 240, key_h, 8, M5.Display.color565(100, 100, 150));
  M5.Display.drawRoundRect(521, special_y+1, 238, key_h-2, 8, M5.Display.color565(80, 80, 120));
  M5.Display.setCursor(548, special_y + 36);
  M5.Display.println("ESPACE");
  
  // BACKSPACE
  M5.Display.fillRoundRect(768, special_y, 256, key_h, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(768, special_y, 256, key_h, 8, RED);
  M5.Display.drawRoundRect(769, special_y+1, 254, key_h-2, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(6);
  M5.Display.setCursor(840, special_y + 27);
  M5.Display.println("<-");
  
  // CANCEL
  M5.Display.fillRoundRect(1026, special_y, 254, key_h, 8, M5.Display.color565(150, 0, 0));
  M5.Display.drawRoundRect(1026, special_y, 254, key_h, 8, RED);
  M5.Display.drawRoundRect(1027, special_y+1, 252, key_h-2, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(6);
  M5.Display.setCursor(1116, special_y + 27);
  M5.Display.println("X");
  
  // Bouton OK (plein écran en bas)
  M5.Display.fillRoundRect(0, 639, 1280, 81, 12, M5.Display.color565(0, 120, 0));
  M5.Display.drawRoundRect(0, 639, 1280, 81, 12, GREEN);
  M5.Display.drawRoundRect(1, 640, 1278, 79, 12, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(520, 654);
  M5.Display.println("OK");
  
  M5.Display.endWrite();  // Fin du buffering
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  
  Serial.begin(115200);
  Serial.println("=== ROOM TEMPERATURE PREDICTION v2.0 ===");
  Serial.println("[SECURITY] Démarrage avec fonctions de sécurité activées");
  
  // Afficher écran de démarrage
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(CYAN);
  M5.Display.setCursor(300, 200);
  M5.Display.println("Demarrage...");
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(350, 280);
  M5.Display.println("Initialisation WiFi...");
  
  // Mode WiFi station
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  Serial.println("[WIFI] Prêt pour connexion manuelle");
  
  // Afficher statistiques de performance initiales
  Serial.println("\n[BOOT] Système démarré - Performance monitoring actif");
  display_performance_stats();
  
  // Afficher le menu
  display_menu();
}

void loop() {
  M5.update();
  
  // Traiter la queue offline toutes les 60 secondes
  static unsigned long last_queue_check = 0;
  if (millis() - last_queue_check > 60000 && queue_count > 0) {
    queue_process();
    last_queue_check = millis();
  }
  
  auto touch = M5.Touch.getDetail();
  
  if (touch.wasPressed()) {
    int x = touch.x;
    int y = touch.y;
    
    // Délai anti-rebond pour éviter les effets visuels
    delay(50);
    
    if (current_page == 0) {  // MENU
      // Manual Prediction (BLUE)
      if (y > 165 && y < 270) {
        current_page = 1;
        display_input();
      }
      // Real WiFi Temp (GREEN)
      else if (y > 285 && y < 390) {
        current_page = 3;
        display_real_temp();
      }
      // Graph Manuel (ORANGE) - Premier tiers
      else if (x >= 40 && x < 440 && y > 405 && y < 510) {
        current_page = 4;
        display_graph_manual();
      }
      // Prediction Temp (CYAN) - Deuxième tiers
      else if (x >= 440 && x < 840 && y > 405 && y < 510) {
        current_page = 9;  // Page 9 = Period Select
        display_period_select();
      }
      // Données CSV (VIOLET) - Troisième tiers (NOUVEAU)
      else if (x >= 840 && x <= 1240 && y > 405 && y < 510) {
        current_page = 10;  // Page 10 = Historical CSV Data
        display_historical_data();
      }
      // WiFi Config (DARKGREY)
      else if (y > 525 && y < 720) {
        current_page = 6;
        display_wifi_config();
      }
    }
    else if (current_page == 1) {  // INPUT (Manual)
      // Boutons DATE/HEURE (y: 45-75)
      if (y >= 45 && y <= 75) {
        // - Jour (x: 150-195)
        if (x >= 150 && x <= 195) {
          manual_day--;
          if (manual_day < 1) manual_day = 31;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // + Jour (x: 265-310)
        else if (x >= 265 && x <= 310) {
          manual_day++;
          if (manual_day > 31) manual_day = 1;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // - Mois (x: 340-385)
        else if (x >= 340 && x <= 385) {
          manual_month--;
          if (manual_month < 1) manual_month = 12;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // + Mois (x: 455-500)
        else if (x >= 455 && x <= 500) {
          manual_month++;
          if (manual_month > 12) manual_month = 1;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // - Heure (x: 650-695)
        else if (x >= 650 && x <= 695) {
          manual_hour--;
          if (manual_hour < 0) manual_hour = 23;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // + Heure (x: 765-810)
        else if (x >= 765 && x <= 810) {
          manual_hour++;
          if (manual_hour > 23) manual_hour = 0;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // - Minute (x: 840-885)
        else if (x >= 840 && x <= 885) {
          manual_minute -= 5;
          if (manual_minute < 0) manual_minute = 55;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
        // + Minute (x: 955-1000)
        else if (x >= 955 && x <= 1000) {
          manual_minute += 5;
          if (manual_minute > 59) manual_minute = 0;
          update_datetime_strings();
          update_datetime_display();  // ✅ Optimisation: mise à jour partielle
        }
      }
      
      // Bouton AUTO : x: 1050-1260, y: 40-80
      if (x >= 1050 && x <= 1260 && y >= 40 && y <= 80) {
        get_current_datetime();
        display_input();
      }
      
      // Bouton - température : x: 0-632, y: 340-395
      if (x < 632 && y >= 340 && y <= 395) {
        current_temp -= 0.1f;
        if (current_temp < -50) current_temp = -50;
        update_temperature_display();  // ✅ Optimisation: mise à jour partielle
      }
      // Bouton + température : x: 648-1280, y: 340-395
      else if (x >= 648 && y >= 340 && y <= 395) {
        current_temp += 0.1f;
        if (current_temp > 60) current_temp = 60;
        update_temperature_display();  // ✅ Optimisation: mise à jour partielle
      }
      // Bouton BACK : x: 0-632, y: 410-480
      else if (x < 632 && y >= 410 && y <= 480) {
        current_page = 0;
        display_menu();
      }
      // Bouton PREDICT : x: 648-1280, y: 410-480
      else if (x >= 648 && y >= 410 && y <= 480) {
        // Mettre à jour les strings avant prédiction
        update_datetime_strings();
        
        predict_rooms(current_temp, 50.0, manual_day, manual_month, predictions);  // humidity par défaut 50%
        add_to_manual_history(current_temp, predictions[0], predictions[1], predictions[2]);
        
        Serial.println("\n=== PREDICTION MANUELLE ===");
        Serial.printf("Date: %s %s\n", manual_date.c_str(), manual_time.c_str());
        Serial.printf("Temp Ext: %.1f C\n", current_temp);
        Serial.printf("Room 1: %.1f C\n", predictions[0]);
        Serial.printf("Room 2: %.1f C\n", predictions[1]);
        Serial.printf("Room 3: %.1f C\n", predictions[2]);
        
        current_page = 2;
        display_result();
      }
    }
    else if (current_page == 2) {  // RESULT (Manual)
      // GRAPH button (zone réduite)
      if (x < 632 && y > 615) {
        current_page = 4;
        display_graph_manual();
      }
      // MENU button (zone réduite)
      else if (x > 648 && y > 615) {
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 3) {  // REAL TEMP (WiFi)
      // Bouton BACK (si WiFi non connecté ou erreur): x: 320-960, y: 550-670
      if (!wifi_connected && x >= 320 && x <= 960 && y >= 550 && y <= 670) {
        current_page = 0;
        display_menu();
      }
      // GRAPH button (si WiFi connecté et données OK)
      else if (wifi_connected && x < 632 && y > 615) {
        current_page = 4;
        display_graph_manual();
      }
      // MENU button (si WiFi connecté et données OK)
      else if (wifi_connected && x > 648 && y > 615) {
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 4) {  // GRAPH MANUAL
      // Flèche GAUCHE du dropdown (changer chambre): x: 880-910, y: 10-60
      if (x >= 880 && x <= 910 && y >= 10 && y <= 60) {
        selected_room_view--;
        if (selected_room_view < 0) selected_room_view = CSV_NUM_ROOMS;  // Boucle
        display_graph_manual();
      }
      // Flèche DROITE du dropdown (changer chambre): x: 1230-1260, y: 10-60
      else if (x >= 1230 && x <= 1260 && y >= 10 && y <= 60) {
        selected_room_view++;
        if (selected_room_view > CSV_NUM_ROOMS) selected_room_view = 0;  // Boucle
        display_graph_manual();
      }
      // Bouton RESET (gauche): x: 50-610, y: 660-720
      else if (x >= 50 && x <= 610 && y >= 660 && y <= 720) {
        reset_manual_history();
        current_page = 0;
        display_menu();
      }
      // Bouton BACK (droite): x: 390-890, y: 550-650
      else if (x >= 390 && x <= 890 && y >= 550 && y <= 650) {
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 5) {  // PREDICTION TEMP
      // Flèche GAUCHE du dropdown (changer chambre): x: 880-910, y: 10-60
      if (x >= 880 && x <= 910 && y >= 10 && y <= 60) {
        selected_room_view--;
        if (selected_room_view < 0) selected_room_view = CSV_NUM_ROOMS;
        display_graph_api();
      }
      // Flèche DROITE du dropdown (changer chambre): x: 1230-1260, y: 10-60
      else if (x >= 1230 && x <= 1260 && y >= 10 && y <= 60) {
        selected_room_view++;
        if (selected_room_view > CSV_NUM_ROOMS) selected_room_view = 0;
        display_graph_api();
      }
      // Bouton BACK: x: 390-890, y: 635-715 (réduit pour ne pas masquer les dates)
      else if (x >= 390 && x <= 890 && y >= 635 && y <= 715) {
        current_page = 9;  // ✅ Page 9 = Period Select (décalé à cause de Saved Networks = page 8)
        display_period_select();
      }
    }
    else if (current_page == 6) {  // WIFI CONFIG
      // Flèche HAUT (défilement vers le haut): x: 1200-1240, y: 70-120
      if (x >= 1200 && x <= 1240 && y >= 70 && y <= 120 && wifi_scroll_offset > 0) {
        wifi_scroll_offset--;
        // Ajuster la sélection si elle sort de la vue
        if (selected_network >= 0 && selected_network < wifi_scroll_offset) {
          selected_network = wifi_scroll_offset;
        }
        display_wifi_config();
      }
      // Flèche BAS (défilement vers le bas): x: 1200-1240, y: 570-620
      else if (x >= 1200 && x <= 1240 && y >= 570 && y <= 620 && wifi_scroll_offset + 6 < scanned_count) {
        wifi_scroll_offset++;
        // Ajuster la sélection si elle sort de la vue
        if (selected_network >= 0 && selected_network >= wifi_scroll_offset + 6) {
          selected_network = wifi_scroll_offset + 5;
        }
        display_wifi_config();
      }
      // Sélection réseau (zones de 102px de hauteur avec offset)
      else if (x < 1190 && y > 69 && y < 687 && scanned_count > 0) {
        int visible_index = (y - 69) / 102;
        if (visible_index < 6) {
          int network_index = visible_index + wifi_scroll_offset;
          if (network_index < scanned_count) {
            selected_network = network_index;
            display_wifi_config();
          }
        }
      }
      // RESCAN button
      else if (x > 20 && x < 310 && y > 630 && y < 705) {
        scan_wifi_networks();
        selected_network = -1;
        wifi_scroll_offset = 0;  // Reset scroll
        display_wifi_config();
      }
      // CONNECT button
      else if (x > 330 && x < 620 && y > 630 && y < 705 && selected_network >= 0) {
        // Copier SSID sélectionné
        scanned_ssids[selected_network].toCharArray(wifi_ssid, 32);
        
        // Ouvrir clavier pour saisir le mot de passe
        keyboard_input = "";
        current_page = 7;
        display_keyboard();
      }
      // BACK button
      else if (x > 640 && x < 950 && y > 630 && y < 705) {
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 7) {  // KEYBOARD
      // Touches du clavier (3 lignes de 10)
      if (y > 180 && y < 486) {
        int row = (y - 180) / 102;
        int col = x / 128;
        
        if (row >= 0 && row < 3 && col >= 0 && col < 10) {
          const char* keys_lower[] = {"azertyuiop", "qsdfghjklm", "wxcvbn,;.!"};
          const char* keys_upper[] = {"AZERTYUIOP", "QSDFGHJKLM", "WXCVBN,;.!"};
          const char* keys_num[] = {"1234567890", "@#$%&*()-+", "[]{}:;\"'<>"};
          
          const char** current_keys = (keyboard_mode == 2) ? keys_num : 
                                       (keyboard_mode == 1) ? keys_upper : keys_lower;
          
          keyboard_input += current_keys[row][col];
          update_keyboard_input_display();  // ✅ Optimisation: mise à jour partielle
        }
      }
      // Touches spéciales (y=486+)
      else if (y > 486 && y < 582) {
        // MODE (ABC/123)
        if (x > 0 && x < 256) {
          keyboard_mode = (keyboard_mode == 2) ? 0 : 2;
          update_keyboard_keys();  // ✅ Optimisation: mise à jour touches uniquement
        }
        // SHIFT (Maj)
        else if (x > 264 && x < 512) {
          if (keyboard_mode != 2) {
            keyboard_mode = (keyboard_mode == 1) ? 0 : 1;
            update_keyboard_keys();  // ✅ Optimisation: mise à jour touches uniquement
          }
        }
        // ESPACE
        else if (x > 520 && x < 760) {
          keyboard_input += " ";
          update_keyboard_input_display();  // ✅ Optimisation: mise à jour partielle
        }
        // BACKSPACE
        else if (x > 768 && x < 1024) {
          if (keyboard_input.length() > 0) {
            keyboard_input.remove(keyboard_input.length() - 1);
            update_keyboard_input_display();  // ✅ Optimisation: mise à jour partielle
          }
        }
        // CANCEL
        else if (x > 1032 && x < 1280) {
          keyboard_input = "";
          current_page = 6;
          display_wifi_config();
        }
      }
      // OK button
      else if (y > 639 && y < 720) {
        // Sauvegarder mot de passe
        keyboard_input.toCharArray(wifi_password, 64);
        
        // Tenter connexion
        connect_wifi();
        delay(2000);
        
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 9) {  // PERIOD SELECT
      // Grille 2×2 (4 boutons de période)
      int btn_w = 580;
      int btn_h = 180;
      int margin_x = 60;
      int margin_y = 220;
      int spacing_x = 640;
      int spacing_y = 200;
      
      // Détection boutons de période (4 boutons)
      for (int i = 0; i < 4; i++) {
        int row = i / 2;
        int col = i % 2;
        
        int btn_x = margin_x + col * spacing_x;
        int btn_y = margin_y + row * spacing_y;
        
        if (x > btn_x && x < btn_x + btn_w && y > btn_y && y < btn_y + btn_h) {
          selected_period = i;
          current_page = 5;
          display_graph_api();
          break;
        }
      }
      
      // Bouton RETOUR
      if (y > 650 && y < 720) {
        current_page = 0;
        display_menu();
      }
    }
    else if (current_page == 10) {  // HISTORICAL DATA (CSV)
      // Zone dropdown période (haut droite): x: 880-1260, y: 10-60
      if (x >= 880 && x <= 1260 && y >= 10 && y <= 60) {
        if (x < 970) {  // Flèche gauche
          csv_period--;
          if (csv_period < 0) csv_period = 3;
          display_historical_data();
        } else if (x > 1170) {  // Flèche droite
          csv_period++;
          if (csv_period > 3) csv_period = 0;
          display_historical_data();
        }
      }
      
      // Zone dropdown room (haut gauche): x: 50-350, y: 10-60
      else if (x >= 50 && x <= 350 && y >= 10 && y <= 60) {
        if (x < 120) {  // Flèche gauche
          selected_room_view--;
          if (selected_room_view < 0) selected_room_view = CSV_NUM_ROOMS;
          display_historical_data();
        } else if (x > 280) {  // Flèche droite
          selected_room_view++;
          if (selected_room_view > CSV_NUM_ROOMS) selected_room_view = 0;
          display_historical_data();
        }
      }
      
      // Bouton BACK: x: 390-890, y: 620-700
      else if (x >= 390 && x <= 890 && y >= 620 && y <= 700) {
        current_page = 0;
        display_menu();
      }
    }
    
    delay(200);
  }
  
  delay(10);
}

