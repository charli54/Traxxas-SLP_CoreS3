#ifndef CONFIG_H
#define CONFIG_H

// --- PARAMÈTRES DE L'ENREGISTREMENT ---
#define FREQUENCE_HZ     20
#define DUREE_SEC        10

// Calcul automatique de la taille du tableau : 20 * 10 = 200
#define BUFFER_SIZE      (FREQUENCE_HZ * DUREE_SEC) 

// --- PARAMÈTRES MATÉRIELS (Optionnel, mais propre) ---
#define SD_CS_PIN        4  // Remplace par ta vraie broche CS si besoin

// Broche GPIO sur laquelle est branché le switch magnétique (Ex: GPIO 1)
// /!\ Choisis une broche libre sur le connecteur du CoreS3
#define MAGNET_SWITCH_PIN 9
#define DISTANCE_INCREMENT 10.205 // en millimètre

// Broches I2C du Port A Grove sur le CoreS3
#define SDA_PIN 2
#define SCL_PIN 1

// Adresse I2C de l'INA226
#define INA226_ADDR 0x40

// Avec un shunt de 1 mOhm, la tension de shunt max autorisée (81.92 mV) donne un courant max de ~81.9 A
#define INA_MAX_CURRENT   80.0f
#define INA_SHUNT_RESISTOR 0.001f

// --- GRAPHIQUE ---
// Couleurs des courbes (et de leur légende)
#define COULEUR_DISTANCE 0xFFFFFF   // blanc
#define COULEUR_COURANT  0x0F00FF   // bleu
#define COULEUR_TENSION  0xFF0000   // rouge

// Pleine échelle minimale de chaque courbe au début d'un enregistrement.
// Elle s'agrandit automatiquement si une mesure la dépasse.
#define ECHELLE_MIN_DISTANCE_M 1.0f
#define ECHELLE_MIN_COURANT_A  5.0f
#define ECHELLE_MIN_TENSION_V  5.0f

#endif // CONFIG_H