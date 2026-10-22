#include <M5Unified.h>
#include <lvgl.h>

#include "config.h"
#include "record.h"
#include "ui/ui.h"
#include "INA226_init.h"

// ── VARIABLES PARTAGÉES ENTRE LES DEUX CŒURS ───────────────────────────
// "volatile" est obligatoire pour les variables modifiées dans une interruption
volatile uint32_t compteurCoups = 0;
static volatile bool enregistrementActif = false;
static volatile bool flagRafraichirGraphique = false;
static volatile bool flagFinEnregistrement = false;

// Nombre de points valides dans les tableaux (écrit par le Cœur 0, lu par le Cœur 1)
static volatile int nbPointsValides = 0;

// Tableaux de mesures BRUTES (remplis par le Cœur 0)
static uint32_t tableauSauvegarde[BUFFER_SIZE];  // nombre de coups de l'encodeur
static float tableauDistance[BUFFER_SIZE];       // en mm
static float tableauCourant[BUFFER_SIZE];        // en A
static float tableauTension[BUFFER_SIZE];        // en V

static int indexTableau = 0;
static int compteurCyclesGraph = 0;

// Tableaux du graphique (déclarés dans ui_scr2.c)
// Chaque série est NORMALISÉE entre 0 et GRAPH_ECHELLE, car distance, courant
// et tension n'ont pas les mêmes unités et ne peuvent pas partager un même axe.
//   cache_serie1 -> Distance (blanc)
//   cache_serie2 -> Courant  (bleu)
//   cache_serie3 -> Tension  (rouge)
extern lv_coord_t cache_serie1[];
extern lv_coord_t cache_serie2[];
extern lv_coord_t cache_serie3[];

#define GRAPH_ECHELLE 1000

// Pleine échelle courante de chaque courbe (s'agrandit pendant l'enregistrement)
static float echelleDistance_m;
static float echelleCourant_A;
static float echelleTension_V;

// Fréquence d'échantillonnage (20 Hz = 50 ms)
const uint32_t frequenceHz = FREQUENCE_HZ;
const uint32_t periodeMs = 1000 / frequenceHz;
TaskHandle_t TaskBackgroundHandle = NULL;

volatile bool isEnregistrementActif(){
    return enregistrementActif;
}

// ── CONFIGURATION DU GRAPHIQUE (à appeler une fois, après ui_init) ─────
// On force les couleurs et les axes depuis le code pour ne pas dépendre de
// SquareLine (qui écrase ui_scr2.c à chaque export).
static void appliquerCouleurSerie(lv_coord_t* tableau, lv_color_t couleur){
    // LVGL insère les séries en tête de liste : on retrouve donc chaque série
    // grâce au tableau qui lui est associé plutôt que par son ordre.
    lv_chart_series_t* serie = lv_chart_get_series_next(ui_graphique, NULL);
    while (serie != NULL) {
        if (serie->y_points == tableau) {
            lv_chart_set_series_color(ui_graphique, serie, couleur);
            return;
        }
        serie = lv_chart_get_series_next(ui_graphique, serie);
    }
}

void initGraph(){
    appliquerCouleurSerie(cache_serie1, lv_color_hex(COULEUR_DISTANCE));
    appliquerCouleurSerie(cache_serie2, lv_color_hex(COULEUR_COURANT));
    appliquerCouleurSerie(cache_serie3, lv_color_hex(COULEUR_TENSION));

    // Les deux axes Y vont de 0 à GRAPH_ECHELLE (valeurs normalisées)
    lv_chart_set_range(ui_graphique, LV_CHART_AXIS_PRIMARY_Y, 0, GRAPH_ECHELLE);
    lv_chart_set_range(ui_graphique, LV_CHART_AXIS_SECONDARY_Y, 0, GRAPH_ECHELLE);

    // Les chiffres 0..1000 des axes Y n'auraient plus de sens : on garde
    // les graduations mais on masque les étiquettes. La pleine échelle de
    // chaque courbe est affichée dans sa légende.
    lv_chart_set_axis_tick(ui_graphique, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 5, 1, false, 0);
    lv_chart_set_axis_tick(ui_graphique, LV_CHART_AXIS_SECONDARY_Y, 5, 0, 5, 1, false, 0);

    // Les légendes prennent exactement la couleur de leur courbe
    lv_obj_set_style_text_color(ui_lbdistance, lv_color_hex(COULEUR_DISTANCE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbCourant,  lv_color_hex(COULEUR_COURANT),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbTension,  lv_color_hex(COULEUR_TENSION),  LV_PART_MAIN | LV_STATE_DEFAULT);

    clearGraph();
}

// Met à jour les légendes avec la pleine échelle de chaque courbe
static void majLegendes(){
    char texte[32];
    snprintf(texte, sizeof(texte), "Distance (0-%.1f m)", echelleDistance_m);
    lv_label_set_text(ui_lbdistance, texte);
    snprintf(texte, sizeof(texte), "Courant (0-%.0f A)", echelleCourant_A);
    lv_label_set_text(ui_lbCourant, texte);
    snprintf(texte, sizeof(texte), "Tension (0-%.1f V)", echelleTension_V);
    lv_label_set_text(ui_lbTension, texte);
}

// Convertit une mesure en point du graphique (0..GRAPH_ECHELLE)
static lv_coord_t normaliser(float valeur, float pleineEchelle){
    if (valeur <= 0.0f) return 0;
    float point = valeur * GRAPH_ECHELLE / pleineEchelle;
    if (point > GRAPH_ECHELLE) point = GRAPH_ECHELLE;
    return (lv_coord_t)point;
}

// Agrandit la pleine échelle (+10 % de marge) si la valeur la dépasse
static bool ajusterEchelle(float valeur, float* pleineEchelle){
    if (valeur > *pleineEchelle) {
        *pleineEchelle = valeur * 1.1f;
        return true;
    }
    return false;
}

void clearGraph() {
    nbPointsValides = 0;
    echelleDistance_m = ECHELLE_MIN_DISTANCE_M;
    echelleCourant_A  = ECHELLE_MIN_COURANT_A;
    echelleTension_V  = ECHELLE_MIN_TENSION_V;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        tableauDistance[i] = 0;
        tableauCourant[i] = 0;
        tableauTension[i] = 0;
        cache_serie1[i] = LV_CHART_POINT_NONE;
        cache_serie2[i] = LV_CHART_POINT_NONE;
        cache_serie3[i] = LV_CHART_POINT_NONE;
    }

    majLegendes();
    lv_chart_refresh(ui_graphique);
}

volatile void setEnregistrementActif(bool actif){
    if(actif == true){
        clearGraph();
        enregistrementActif = true;
    }
    else{
        enregistrementActif = false;
    }
}

const uint32_t* getTableauSauvegarde(){
    return tableauSauvegarde;
}

// ── MISE À JOUR DU GRAPHIQUE (CŒUR 1, appelée dans loop) ───────────────
// Toutes les fonctions LVGL sont appelées ici, jamais depuis le Cœur 0.
void updateGraph(){
    if (flagRafraichirGraphique) {
        flagRafraichirGraphique = false;

        int n = nbPointsValides;

        // 1. On ajuste les pleines échelles d'après les nouvelles mesures
        bool echelleChangee = false;
        for (int i = 0; i < n; i++) {
            echelleChangee |= ajusterEchelle(tableauDistance[i] / 1000.0f, &echelleDistance_m);
            echelleChangee |= ajusterEchelle(tableauCourant[i], &echelleCourant_A);
            echelleChangee |= ajusterEchelle(tableauTension[i], &echelleTension_V);
        }
        if (echelleChangee) majLegendes();

        // 2. On recalcule toutes les courbes (200 points, c'est négligeable)
        for (int i = 0; i < n; i++) {
            cache_serie1[i] = normaliser(tableauDistance[i] / 1000.0f, echelleDistance_m);
            cache_serie2[i] = normaliser(tableauCourant[i], echelleCourant_A);
            cache_serie3[i] = normaliser(tableauTension[i], echelleTension_V);
        }

        lv_chart_refresh(ui_graphique);
    }

    // 3. Fin d'enregistrement (buffer plein) : mise à jour des boutons
    if (flagFinEnregistrement) {
        flagFinEnregistrement = false;
        lv_bar_set_value(ui_btRecordData, 0, LV_ANIM_OFF);
        lv_obj_clear_state(ui_btRecordData, LV_STATE_CHECKED);
        lv_obj_clear_state(ui_btSaveData, LV_STATE_DISABLED);
        lv_obj_set_style_opa(ui_btSaveData, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_state(ui_btHomeScr2, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_btSaveScr2, LV_STATE_DISABLED);
    }
}

// ── ROUTINE D'INTERRUPTION (ISR) ───────────────────────────────────────
// Cette fonction ultra-rapide s'exécute à CHAQUE front (montant et descendant)
void IRAM_ATTR onSwitchChange() {
    compteurCoups++;
}

// ── LA TÂCHE DE FOND (THREAD SUR CŒUR 0) ───────────────────────────────
// Elle ne fait QUE mesurer et remplir les tableaux. Aucun appel LVGL ici.
void MaTacheDeFond(void * pvParameters) {
    for(;;) {

        if(enregistrementActif){

            // --- 1. Lecture des mesures ---
            uint32_t valeurActuelle = compteurCoups;

            tableauSauvegarde[indexTableau] = valeurActuelle;
            tableauDistance[indexTableau] = valeurActuelle * DISTANCE_INCREMENT; // mm
            tableauCourant[indexTableau]  = INA.getCurrent();                    // A
            tableauTension[indexTableau]  = INA.getBusVoltage();                 // V

            indexTableau++;
            nbPointsValides = indexTableau;

            // --- 2. Rafraîchissement du graphique à 5 Hz (1 cycle sur 4) ---
            compteurCyclesGraph++;
            if (compteurCyclesGraph >= 4) {
                compteurCyclesGraph = 0;
                flagRafraichirGraphique = true;
            }

            // --- 3. Buffer plein : fin de l'enregistrement ---
            if (indexTableau >= BUFFER_SIZE) {
                enregistrementActif = false;
                flagRafraichirGraphique = true;   // dernier affichage complet
                flagFinEnregistrement = true;     // le Cœur 1 met à jour les boutons
                indexTableau = 0;
                compteurCoups = 0;
                compteurCyclesGraph = 0;
            }
        }
        else{
            indexTableau = 0;
            compteurCyclesGraph = 0;
            compteurCoups = 0;
        }
        // --- Timing à 20 Hz (50 ms) ---
        vTaskDelay(pdMS_TO_TICKS(periodeMs));
    }
}

void initInterruptmagneticSwitch(){
    //On active le PULLUP pour travailler à une tension de 3.3V
    pinMode(MAGNET_SWITCH_PIN, INPUT_PULLUP);

    // CHANGE : Déclenche l'interruption sur front montant ET descendant
    attachInterrupt(digitalPinToInterrupt(MAGNET_SWITCH_PIN), onSwitchChange, CHANGE);
}

void initThreadCalculEncodeur(){
    // Création de la tâche FreeRTOS bloquée sur le Cœur 0
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