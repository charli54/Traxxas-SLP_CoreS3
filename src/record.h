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

const uint32_t* getTableauSauvegarde(); 


#ifdef __cplusplus
}
#endif

#endif