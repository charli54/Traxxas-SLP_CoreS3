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

// Échelle de tension de départ (axe de droite). Si la batterie est plus haute
// que TENSION_HAUT_V au début de l'enregistrement, on part de sa tension.
// Si la tension descend sous TENSION_BAS_V (chute sous charge), l'axe s'élargit vers le bas.
#define TENSION_HAUT_V 9.0f
#define TENSION_BAS_V  7.0f

// --- FICHIER CSV ---
// 1 : virgule décimale (12,345), pratique avec Excel en français et le séparateur ';'
// 0 : point décimal (12.345)
#define CSV_VIRGULE_DECIMALE 1

// --- HORLOGE (RTC) ---
// Par défaut, le RTC est réglé automatiquement à l'heure du PC au moment de
// la compilation, au premier démarrage après chaque nouveau flash.
// RTC_DECALAGE_SEC compense le temps entre la compilation et le démarrage
// (upload + boot). Ajuste-le si l'heure est systématiquement en retard.
#define RTC_DECALAGE_SEC 20

// Réglage manuel : mettre RTC_REGLAGE_MANUEL à 1, saisir l'heure locale
// voulue, puis flasher. L'heure est appliquée une seule fois (au premier
// démarrage avec ces valeurs), pas à chaque redémarrage.
#define RTC_REGLAGE_MANUEL 0
#define RTC_ANNEE   2026
#define RTC_MOIS    10
#define RTC_JOUR    22
#define RTC_HEURE   14
#define RTC_MINUTE  30

#endif // CONFIG_H