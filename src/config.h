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

#endif // CONFIG_H