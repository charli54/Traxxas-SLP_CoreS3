#ifndef RECORD_H
#define RECORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>

volatile bool isEnregistrementActif(void);
volatile void setEnregistrementActif(bool actif);
void initInterruptmagneticSwitch(void);
void initThreadCalculEncodeur(void);
void updateGraph(void);
void clearGraph(void);
void initGraph(void);

// Accès aux mesures du dernier enregistrement (pour la sauvegarde CSV)
int getNbPoints(void);
const uint32_t* getTableauSauvegarde(void);   // coups de l'encodeur
const uint32_t* getTableauTemps(void);        // ms
const float* getTableauDistance(void);        // mm
const float* getTableauCourant(void);         // A
const float* getTableauTension(void);         // V


#ifdef __cplusplus
}
#endif

#endif