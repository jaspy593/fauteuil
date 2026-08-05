#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

const char* ssid = "MIT-Grande-Salle";
const char* password = "!321poiuytreza";
WiFiServer server(5000);
WiFiClient client;

MAX30105 particleSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t bufferLength = 100;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

const int IN1 = 18;
const int IN2 = 19;
const int IN3 = 21;
const int IN4 = 22;
const int ENA = 23;
const int ENB = 5;
const int TRIG = 12;
const int ECHO = 13;

unsigned long dernierEnvoi = 0;
int sampleCounter = 0; // Compteur pour savoir quand recalculer le BPM/SpO2

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté !");
  Serial.println(WiFi.localIP()); 
  server.begin();
  
  Wire.begin(2, 4);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 non détecté");
    while(1) delay(1000);
  }
  Serial.println("MAX30102 détecté");
  
  particleSensor.setup(80, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  ledcAttach(ENA, 5000, 8);
  ledcAttach(ENB, 5000, 8);

  // Pré-remplir le buffer au démarrage pour initialiser l'algorithme
  Serial.println("Initialisation du buffer de données...");
  for (int i = 0; i < bufferLength; i++) {
    while (!particleSensor.available()) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  
  // Premier calcul initial
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
}

void loop() {
  connexion();
  lireCapteur();

  // Envoi vers Qt toutes les 1 seconde (1000ms) sans bloquer
  if (millis() - dernierEnvoi >= 500) {
    dernierEnvoi = millis();
    envoyerDonnees();
  }
}

void lireCapteur() {
  particleSensor.check(); // Vérifie si le capteur a de nouvelles données
  
  if (particleSensor.available()) {
    // Décale toutes les anciennes données vers la gauche (libère la dernière place)
    for (int i = 1; i < bufferLength; i++) {
      redBuffer[i - 1] = redBuffer[i];
      irBuffer[i - 1] = irBuffer[i];
    }
    
    // Ajoute la nouvelle mesure à la fin du buffer
    redBuffer[bufferLength - 1] = particleSensor.getRed();
    irBuffer[bufferLength - 1] = particleSensor.getIR();
    particleSensor.nextSample();
    
    sampleCounter++;
    
    // On recalcule le BPM/SpO2 tous les 25 nouveaux échantillons (toutes les 250ms)
    // pour ne pas surcharger le processeur à chaque micro-seconde
    if (sampleCounter >= 25) {
      sampleCounter = 0;
      
      maxim_heart_rate_and_oxygen_saturation(
        irBuffer, bufferLength, redBuffer, 
        &spo2, &validSPO2, &heartRate, &validHeartRate
      );

      // Affichage de debug rapide sur le port série
      Serial.print("BPM: "); Serial.print(validHeartRate ? String(heartRate) : "--");
      Serial.print(" | SpO2: "); Serial.println(validSPO2 ? String(spo2) : "--");
    }
  }
}

// ---- Vos fonctions de mouvements et de connexion restent identiques ----
void avancer() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  vitesse(80);
}
void reculer() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  vitesse(80);
}
void gauche() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  vitesse(80);
}
void droite() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  vitesse(80);
}
void arreter() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  vitesse(0);
}
void vitesse(int v) {
  ledcWrite(ENA, v); ledcWrite(ENB, v);
}
float mesure_distance() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10); 
  digitalWrite(TRIG, LOW);
  long temps = pulseIn(ECHO, HIGH);
  return (temps / 58.0);
}
void evite_obstacle() {
  float distance = mesure_distance();
  if (distance < 10) { reculer(); delay(1000); arreter(); delay(1000); }
  else if (distance < 30) { arreter(); }
  else { avancer(); }
}
void connexion() {
  if (!client || !client.connected()) {
    client = server.available();
    if (client) Serial.println("PC connecté !");
  }
  if (client && client.connected() && client.available()) {
    String commande = client.readStringUntil('\n');
    commande.trim();
    Serial.print("Commande reçue : "); Serial.println(commande);
    execute_commande(commande);
  }
}
void execute_commande(String commande) {
  if (commande == "avancer") evite_obstacle();
  else if (commande == "reculer") reculer();
  else if (commande == "droite") droite();
  else if (commande == "gauche") gauche();
  else if (commande == "arreter") arreter();
  else Serial.println("Commande inconnue !");
}
void envoyerDonnees() {
  if (!client || !client.connected()) return;
  client.print("BPM:");
  client.print(validHeartRate ? String(heartRate) : "--");
  client.print(";SPO2:");
  client.print(validSPO2 ? String(spo2) : "--");
  client.println();
  Serial.println("Données envoyées à Qt.");
}
