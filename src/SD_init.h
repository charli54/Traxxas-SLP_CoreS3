#ifndef SD_INIT_H
#define SD_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool usbEjected;
extern volatile bool usbPretEtMonte;

bool sdInit(void);
void enableUsbMode();
void disableUsbMode();
bool isUsbModeActive();
void sauvegarderDonnees(uint32_t *tableau, int taille);
bool isUSBReady(void);
//const char* genererNomFichierUnique(const char* nomDeBase, const char* extension);
//String genererNomFichierUnique(String nomDeBase, String extension);
const char* imprimerDossier();
const char* getListFichierCarteSD();

#ifdef __cplusplus
}
#endif

#endif