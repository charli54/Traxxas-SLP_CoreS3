
#include <M5Unified.h>
#include <lvgl.h>

#include "config.h"
#include "record.h"
#include "ui/ui.h"
#include "INA226_init.h"

// ── VARIABLES PARTAGÉES (INDICATIONS ARDUINO/FREERTOS) ─────────────────
// "volatile" est obligatoire pour les variables modifiées dans une interruption
volatile uint32_t compteurCoups = 0;
volatile uint32_t limiteYMaxActuelle = 1000;
static volatile bool enregistrementActif = false;
static volatile bool flagChangerEchelleY = false;

// Tableau pour stocker les échantillons
static uint32_t tableauSauvegarde[BUFFER_SIZE];
static float tableauDistance[BUFFER_SIZE];

int indexTableau = 0;

// Variables globales de partage (à placer en haut du code)
volatile bool flagRafraichirGraphique = false;
int compteurCyclesGraph = 0;

// Les tableaux partagés (Mode Chenillard - Taille 200)
extern lv_coord_t cache_serie1[];   //declarée dans scr2.cpp
extern lv_coord_t cache_serie2[];
extern lv_coord_t cache_serie3[];

bool lastEnregistrementActif = false;

// Fréquence d'échantillonnage (20 Hz = 50 ms)
const uint32_t frequenceHz = FREQUENCE_HZ;
const uint32_t periodeMs = 1000 / frequenceHz; 
TaskHandle_t TaskBackgroundHandle = NULL;

volatile bool isEnregistrementActif(){
    return enregistrementActif;
}

void clearGraph() {
    // 1. On remet l'index d'écriture au début (à gauche)
    indexTableau = 0; 
    limiteYMaxActuelle = 1000;
    lv_chart_set_range(ui_graphique, LV_CHART_AXIS_PRIMARY_Y, 0, limiteYMaxActuelle);

    
    // 2. On remplit le tableau du graphique avec la valeur "invisible"
    for (int i = 0; i < BUFFER_SIZE; i++) {
        tableauDistance[i] = 0;
        cache_serie1[i] = LV_CHART_POINT_NONE;
        cache_serie2[i] = LV_CHART_POINT_NONE;
        cache_serie3[i] = LV_CHART_POINT_NONE;
    }
    
    // 3. On rafraîchit pour effacer l'ancien graphique
    lv_chart_refresh(ui_graphique);
    
}

volatile void setEnregistrementActif(bool actif){
    if(actif == true){
        clearGraph();
        enregistrementActif = true;
    }
    else{
        enregistrementActif = false;
        lastEnregistrementActif = false;
    }
}

const uint32_t* getTableauSauvegarde(){
    return tableauSauvegarde;
}


void updateGraph(){
  // 2. Vérification du drapeau de rafraîchissement du Graphique (5 Hz)
  
  
  if (flagRafraichirGraphique) {
    if (flagChangerEchelleY) {
        flagChangerEchelleY = false;

        // On applique la nouvelle échelle calculée par le Cœur 0
        lv_chart_set_range(ui_graphique, LV_CHART_AXIS_PRIMARY_Y, 0, limiteYMaxActuelle);
    }

    flagRafraichirGraphique = false; // On baisse immédiatement le drapeau
    
    // On demande à LVGL de redessiner la courbe avec le tableau mis à jour par le Cœur 0
    lv_chart_refresh(ui_graphique); 
}

  // 3. Détection de la fin de l'enregistrement (Quand le buffer est plein)
  // Le Cœur 0 a passé 'enregistrementActif' à 0, on met à jour l'UI ici
  if (enregistrementActif == 0 && lastEnregistrementActif == true) {
      
      // On peut maintenant modifier nos labels et boutons sans risque de crash !
      //lv_label_set_text_fmt(ui_lbdebugScr2, "%d", BUFFER_SIZE);
      
      // Exemple : Remettre ton bouton REC à l'état décoché si besoin
      // lv_obj_clear_state(ui_btRecord, LV_STATE_CHECKED);
  }
  
  // On mémorise l'état pour le prochain cycle
  lastEnregistrementActif = enregistrementActif;
}

// ── ROUTINE D'INTERRUPTION (ISR) ───────────────────────────────────────
// Cette fonction ultra-rapide s'exécute à CHAQUE front (montant et descendant)
void IRAM_ATTR onSwitchChange() {
    compteurCoups++;
}

// ── LA TÂCHE DE FOND (THREAED SUR CŒUR 0) ──────────────────────────────
void MaTacheDeFond(void * pvParameters) {
    //USBSerial.printf("[Thread] Démarré sur le cœur : %d à %d Hz\n", xPortGetCoreID(), frequenceHz);

    for(;;) {

        if(enregistrementActif){

            // --- 1. Lecture et Sauvegarde de la valeur ---
            uint32_t valeurActuelle = compteurCoups;
            float distanceActuelle = compteurCoups * DISTANCE_INCREMENT;
                // Lecture des mesures de l'INA226
            float busVoltage_mV = INA.getBusVoltage() * 1000;       // Tension du bus en Volts
            float current_mA    = INA.getCurrent() * 1000;          // Courant traversant le shunt en Ampères
            //float power_W      = INA.getPower();            // Puissance calculée en Watts
            //float shuntVoltage_mV = INA.getShuntVoltage() * 1000.0; // Tension aux bornes du shunt en mV

            tableauSauvegarde[indexTableau] = valeurActuelle;
            tableauDistance[indexTableau] = distanceActuelle;
            

            cache_serie1[indexTableau] = (lv_coord_t)valeurActuelle;
            cache_serie2[indexTableau] = (lv_coord_t)current_mA;
            cache_serie3[indexTableau] = (lv_coord_t)busVoltage_mV;


            // Si la nouvelle valeur dépasse notre limite actuelle (avec une marge de sécurité de 10% par exemple)
            if (distanceActuelle > limiteYMaxActuelle) {
                // On augmente la limite pour donner de l'air au graphique
                limiteYMaxActuelle = distanceActuelle + (distanceActuelle / 10); 
                
                // On lève un drapeau pour dire au Cœur 1 de mettre à jour l'échelle
                flagChangerEchelleY = true; 
            }

            // --- 3. Gestion de la Fréquence d'Affichage (Diviseur par 4 -> 5Hz) ---
            compteurCyclesGraph++;
            if (compteurCyclesGraph >= 4) { 
                compteurCyclesGraph = 0;
                flagRafraichirGraphique = true; // On lève le drapeau pour le Cœur 1
            }
            indexTableau++;
            if (indexTableau >= BUFFER_SIZE) {
                
                //stopperEnregistrement();
                enregistrementActif = 0;
                indexTableau = 0; // On recommence au début (ou tu traites tes données ici)
                compteurCoups = 0;
                compteurCyclesGraph = 0;
                lv_bar_set_value(ui_btRecordData, 0, LV_ANIM_OFF);
                lv_obj_clear_state(ui_btRecordData, LV_STATE_CHECKED);
                lv_obj_clear_state(ui_btSaveData, LV_STATE_DISABLED);
                lv_obj_set_style_opa(ui_btSaveData, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_state(ui_btHomeScr2, LV_STATE_DISABLED);
                lv_obj_clear_state(ui_btSaveScr2, LV_STATE_DISABLED);

            }
        }
        else{
            indexTableau = 0;
            compteurCyclesGraph = 0;
            compteurCoups = 0;
        }
        // --- 3. Timing précis à 20 Hz (50 ms) ---
        // pdMS_TO_TICKS convertit automatiquement les millisecondes en cycles FreeRTOS
        vTaskDelay(pdMS_TO_TICKS(periodeMs)); 
    }
}

void initInterruptmagneticSwitch(){
    //On active le PULLUP pour travailler à une tension de 3.3V
    pinMode(MAGNET_SWITCH_PIN, INPUT_PULLUP);

    // 2. Attacher l'interruption sur la broche
    // CHANGE : Déclenche l'interruption sur front montant ET descendant
    attachInterrupt(digitalPinToInterrupt(MAGNET_SWITCH_PIN), onSwitchChange, CHANGE);
}

void initThreadCalculEncodeur(){
    // 3. Création de la tâche FreeRTOS bloquée sur le Cœur 0
    xTaskCreatePinnedToCore(
        MaTacheDeFond,          // Fonction
        "TaskBackground",       // Nom
        4096,                   // Pile (Stack)
        NULL,                   // Paramètres
        1,                      // Priorité
        &TaskBackgroundHandle,  // Handle
        0                       // Cœur 0
    );
}