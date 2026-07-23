#include <SD.h>
#include <SPI.h>
#include <USB.h>
#include <USBMSC.h>

#include "SD_init.h"
#include "config.h"
#include "ui/ui.h"


USBMSC msc;
bool usbMode = false;
//------------Pour gérer l'ejection de l'USB------------------
volatile bool usbEjected = false;
volatile bool usbPretEtMonte = false;
bool firstMount = true;

static String listeDesFichierCarteSD = "";

// Fonction de lecture corrigée pour gérer le multi-secteurs
static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    uint32_t sector_count = bufsize / 512;
    uint8_t* buf = (uint8_t*)buffer;



    for (uint32_t i = 0; i < sector_count; i++) {
        // On avance le pointeur de buffer de 512 octets et on incrémente le secteur (lba)
        if (!SD.readRAW(buf + (i * 512), lba + i)) {
            return -1; // Erreur de lecture, on prévient le PC
        }
    }

     if(firstMount){
        firstMount = false;
        usbPretEtMonte = true;
    }

    return bufsize; // Tout s'est bien passé
}

// Fonction d'écriture corrigée pour la lecture seule
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    uint32_t sector_count = bufsize / 512;
    uint8_t* buf = (uint8_t*)buffer;

    for (uint32_t i = 0; i < sector_count; i++) {
        if (!SD.writeRAW(buf + (i * 512), lba + i)) {
            return -1;
        }
    }
    return bufsize;
}

bool sdInit(){
    SPI.begin(SCK, MISO, MOSI, SD_CS_PIN);
        if (!SD.begin(SD_CS_PIN, SPI, 20000000)) { 
            
            return false;
        }
    return true;
}

extern "C" bool isUSBReady(){
    return (bool)USB;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    // Si 'start' est faux, cela signifie que le PC demande l'arrêt/l'éjection du lecteur
    if (!start) {
        usbEjected = true;  
    }

    return true;
}

void enableUsbMode() {

    uint32_t sectors = SD.numSectors();
    uint32_t sectorSize = SD.sectorSize();

    msc.vendorID("M5Stack");
    msc.productID("CoreS3 SD");
    msc.productRevision("1.0");
    
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    
    msc.mediaPresent(true); // Signale que la carte est là
    msc.begin(sectors, sectorSize);
    USB.begin();

    usbMode = true;
    usbEjected = false;

}

// ── PASSER EN MODE ESP32 (Écriture locale) ────────────────────────
void disableUsbMode() {
    if (!usbMode) return; // Déjà désactivé
    firstMount = true;

    // 1. On signale au PC que la carte a été retirée "physiquement"
    msc.mediaPresent(false);
    delay(500); // Laisse le temps au PC de comprendre le débranchement

    // 2. On éteint proprement la pile USB MSC
    msc.end();
    
    // 3. (Optionnel mais recommandé) On rafraîchit le système de fichier local ESP32
    SD.end();
    delay(100);
    SD.begin(SD_CS_PIN, SPI, 20000000);
    
    lv_obj_clear_state(ui_btSauvegarde, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_opa(ui_btSauvegarde, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_clear_state(ui_btRecord, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_opa(ui_btRecord, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_state(ui_btGraphScr, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_clear_state(ui_btSaveScr, LV_PART_MAIN | LV_STATE_DISABLED);
    //pour le Screen4
    lv_obj_clear_state(ui_btRecord1, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_opa(ui_btRecord1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_clear_state(ui_btGraphScr4, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_clear_state(ui_btHomeScr4, LV_PART_MAIN | LV_STATE_DISABLED);

    usbPretEtMonte = false;
    usbMode = false;
}

// Fonction pour savoir dans quel mode on est
bool isUsbModeActive() {
    return usbMode;
}

/*
const char* genererNomFichierUnique(const char* base, const char* ext) {
    
    String nomDeBase = String(base);
    String extension = String(ext);
    
    // 2. On déclare la String de sortie en STATIC pour qu'elle survive à la fin de la fonction
    static String nomFichierFinal;    

    // 1. On teste d'abord le fichier sans index (ex: "/donnees.csv")
    nomFichierFinal = nomDeBase + extension;
    
    if (!SD.exists(nomFichierFinal)) {
        return nomFichierFinal.c_str(); // Le fichier n'existe pas, on peut l'utiliser direct !
    }

    // 2. Si le fichier de base existe, on cherche un index libre (ex: _2, _3, _4...)
    int compteur = 2;
    while (true) {
        nomFichierFinal = nomDeBase + "_" + String(compteur) + extension;
        
        if (!SD.exists(nomFichierFinal)) {
            // On a trouvé un emplacement libre !
            break; 
        }
        compteur++; // Sinon, on passe au numéro suivant
    }

    //on retire l'Extension
    int indexPoint = nomFichierFinal.lastIndexOf('.');

    String nomFichier = nomFichierFinal.substring(0, indexPoint);

    return nomFichier.c_str();
}

void sauvegarderDonnees(uint32_t *tableau, int taille, const char* name) {
    if (taille == 0 || tableau == NULL) {
            //rien a sauvegarder
        return;
    }
    String uniqueFileName = "/" + String(name) + ".csv";

    //String uniqueFileName = genererNomFichierUnique("/data", ".csv");

    File fichier = SD.open(uniqueFileName, FILE_WRITE);
    
    if (fichier) {
        //if (fichier.size() == 0) {
        //    fichier.println("Index;Valeur,");
        }//

        fichier.println("Time(ms);value(m)");
        // On parcourt le tableau reçu en argument
        for (int i = 0; i < taille; i++) {
            fichier.printf("%d;%d\n", (i+1)*50, tableau[i]);
        }
        
        fichier.close(); 
        //USBSerial.printf("Sauvegarde réussie : %d lignes écrites.\n", taille);
        //screen1_setLabelSaveState("sauvé");
    } else {
        //USBSerial.println("Erreur : Impossible d'ouvrir le fichier sur la carte SD.");
        //screen1_setLabelSaveState("Not Open");
    }
}*/

String genererNomFichierUnique(String nomDeBase, String extension) {
    // 1. On teste d'abord le fichier sans index (ex: "/donnees.csv")
    String nomComplet = nomDeBase + extension;
    
    if (!SD.exists(nomComplet)) {
        return nomComplet; // Le fichier n'existe pas, on peut l'utiliser direct !
    }

    // 2. Si le fichier de base existe, on cherche un index libre (ex: _2, _3, _4...)
    int compteur = 2;
    while (true) {
        nomComplet = nomDeBase + String(compteur) + extension;
        
        if (!SD.exists(nomComplet)) {
            // On a trouvé un emplacement libre !
            break; 
        }
        compteur++; // Sinon, on passe au numéro suivant
    }

    return nomComplet;
}

void sauvegarderDonnees(uint32_t *tableau, int taille) {
    if (taille == 0 || tableau == NULL) {
            //screen1_setLabelSaveState("tab Vide");
        return;
    }
    String uniqueFileName = genererNomFichierUnique("/Traxxas", ".csv");

    File fichier = SD.open(uniqueFileName, FILE_WRITE);
    
    if (fichier) {
        /*if (fichier.size() == 0) {
            fichier.println("Index;Valeur,");
        }*/

        fichier.println("Time(ms);value(m)");
        // On parcourt le tableau reçu en argument
        for (int i = 0; i < taille; i++) {
            fichier.printf("%d;%d\n", (i+1)*50, tableau[i]);
        }
        
        fichier.close(); 
        //USBSerial.printf("Sauvegarde réussie : %d lignes écrites.\n", taille);
        //screen1_setLabelSaveState("sauvé");
    } else {
        //USBSerial.println("Erreur : Impossible d'ouvrir le fichier sur la carte SD.");
        //screen1_setLabelSaveState("Not Open");
    }
}

const char* imprimerDossier() {

    listeDesFichierCarteSD = "";
    // Ouvre le dossier racine ("/")
    File root = SD.open("/");
    //static String listeDesFichiers;
    if (!root) return "Erreur: SD non montée";

    while (true) {
        File entry =  root.openNextFile();
        if (! entry) {
        // Plus de fichiers dans ce dossier
        break;
        }
        
        if (strstr(entry.name(), "System Volume") == nullptr){
            String nomFichier = entry.name();

            //on retire l'Extension
            int indexPoint = nomFichier.lastIndexOf('.');

            listeDesFichierCarteSD += nomFichier.substring(0, indexPoint) + "\n";
        }
        
        // Libère la mémoire occupée par le fichier
        entry.close();
    }

    root.close();
    //return "saved";
    return listeDesFichierCarteSD.c_str();
}

const char* getListFichierCarteSD(){
    return listeDesFichierCarteSD.c_str();
}