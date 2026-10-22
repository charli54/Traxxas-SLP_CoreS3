#include <M5Unified.h>
#include <lvgl.h>
#include <math.h>

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
static uint32_t tableauTemps[BUFFER_SIZE];       // en ms depuis le début de l'enregistrement
static float tableauDistance[BUFFER_SIZE];       // en mm
static float tableauCourant[BUFFER_SIZE];        // en A
static float tableauTension[BUFFER_SIZE];        // en V

static int indexTableau = 0;
static int compteurCyclesGraph = 0;
static uint32_t debutEnregistrementMs = 0;

// Tableaux du graphique (déclarés dans ui_scr2.c)
//   cache_serie1 -> Distance (blanc) - axe de gauche, mise à l'échelle sans graduation
//   cache_serie2 -> Courant  (bleu)  - axe de gauche, en centièmes d'ampère
//   cache_serie3 -> Tension  (rouge) - axe de droite, en millivolts
extern lv_coord_t cache_serie1[];
extern lv_coord_t cache_serie2[];
extern lv_coord_t cache_serie3[];

// Unités internes du graphique (lv_coord_t est un entier 16 bits : max 32767)
#define UNITE_COURANT 100.0f    // 1 A  = 100 points  (max ~327 A)
#define UNITE_TENSION 1000.0f   // 1 V  = 1000 points (max ~32 V)
#define NB_DIVISIONS_Y 4        // 4 divisions = 5 graduations avec valeur

// Échelles actuelles (modifiées uniquement par le Cœur 1)
static float echelleDistance_m;     // pleine échelle de la distance
static float echelleCourant_A;      // l'axe de gauche va de 0 à echelleCourant_A
static float pasCourant_A;          // écart entre deux graduations
static float tensionBas_V;          // l'axe de droite va de tensionBas_V ...
static float tensionHaut_V;         // ... à tensionHaut_V
static float pasTension_V;

// Fréquence d'échantillonnage (20 Hz = 50 ms)
const uint32_t frequenceHz = FREQUENCE_HZ;
const uint32_t periodeMs = 1000 / frequenceHz;
TaskHandle_t TaskBackgroundHandle = NULL;

volatile bool isEnregistrementActif(){
    return enregistrementActif;
}

// ── ACCÈS AUX DONNÉES (pour la sauvegarde CSV) ─────────────────────────
int getNbPoints(){ return nbPointsValides; }
const uint32_t* getTableauSauvegarde(){ return tableauSauvegarde; }
const uint32_t* getTableauTemps(){ return tableauTemps; }
const float* getTableauDistance(){ return tableauDistance; }
const float* getTableauCourant(){ return tableauCourant; }
const float* getTableauTension(){ return tableauTension; }

// ── CALCUL D'ÉCHELLES « PROPRES » ──────────────────────────────────────
// Arrondit un pas à 1, 2, 2.5 ou 5 x 10^n pour avoir des graduations lisibles
// (ex. 0 / 5 / 10 / 15 / 20 A plutôt que 0 / 5.5 / 11 / 16.5 / 22 A).
static float pasArrondi(float pasBrut){
    if (pasBrut <= 0.0f) return 1.0f;
    float puissance = powf(10.0f, floorf(log10f(pasBrut)));
    float n = pasBrut / puissance;
    float m;
    if      (n <= 1.0f)  m = 1.0f;
    else if (n <= 2.0f)  m = 2.0f;
    else if (n <= 2.5f)  m = 2.5f;
    else if (n <= 5.0f)  m = 5.0f;
    else                 m = 10.0f;
    return m * puissance;
}

// Courant : axe de 0 à une valeur ronde qui contient courantMax
static void calculerEchelleCourant(float courantMax){
    pasCourant_A = pasArrondi(courantMax / NB_DIVISIONS_Y);
    echelleCourant_A = pasCourant_A * NB_DIVISIONS_Y;
}

// Tension : axe [bas ; haut] arrondi qui contient [vMin ; vMax]
static void calculerEchelleTension(float vMin, float vMax){
    float pas = pasArrondi((vMax - vMin) / NB_DIVISIONS_Y);
    float bas = floorf(vMin / pas + 0.001f) * pas;
    // Si l'arrondi du bas fait dépasser le haut, on prend le pas suivant
    while (bas + pas * NB_DIVISIONS_Y < vMax - 0.001f) {
        pas = pasArrondi(pas * 1.01f);
        bas = floorf(vMin / pas + 0.001f) * pas;
    }
    pasTension_V = pas;
    tensionBas_V = bas;
    tensionHaut_V = bas + pas * NB_DIVISIONS_Y;
}

static void appliquerEchelles(){
    lv_chart_set_range(ui_graphique, LV_CHART_AXIS_PRIMARY_Y,
                       0, (lv_coord_t)lroundf(echelleCourant_A * UNITE_COURANT));
    lv_chart_set_range(ui_graphique, LV_CHART_AXIS_SECONDARY_Y,
                       (lv_coord_t)lroundf(tensionBas_V * UNITE_TENSION),
                       (lv_coord_t)lroundf(tensionHaut_V * UNITE_TENSION));
}

// Nombre de décimales nécessaires pour afficher un pas (5 -> 0, 0.5 -> 1, 0.25 -> 2)
static int decimalesPour(float pas){
    int d = 0;
    float p = pas;
    while (d < 3 && fabsf(p - roundf(p)) > 0.001f) { p *= 10.0f; d++; }
    return d;
}

// ── ÉTIQUETTES DES AXES ────────────────────────────────────────────────
// LVGL écrit par défaut la valeur brute (centièmes d'A, mV). On la remplace
// par la valeur en A / V et on colore chaque axe comme sa courbe.
static void formaterEtiquettesAxes(lv_event_t* e){
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_TICKS || dsc->text == NULL) return;

    if (dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
        snprintf(dsc->text, dsc->text_length, "%.*f",
                 decimalesPour(pasCourant_A), dsc->value / UNITE_COURANT);
        dsc->label_dsc->color = lv_color_hex(COULEUR_COURANT);
    }
    else if (dsc->id == LV_CHART_AXIS_SECONDARY_Y) {
        snprintf(dsc->text, dsc->text_length, "%.*f",
                 decimalesPour(pasTension_V), dsc->value / UNITE_TENSION);
        dsc->label_dsc->color = lv_color_hex(COULEUR_TENSION);
    }
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

    // 5 graduations avec valeur à gauche (courant) et à droite (tension).
    // Le dernier paramètre réserve la place des étiquettes hors du graphique.
    lv_chart_set_axis_tick(ui_graphique, LV_CHART_AXIS_PRIMARY_Y,   4, 2, NB_DIVISIONS_Y + 1, 2, true, 30);
    lv_chart_set_axis_tick(ui_graphique, LV_CHART_AXIS_SECONDARY_Y, 4, 2, NB_DIVISIONS_Y + 1, 2, true, 30);
    lv_obj_add_event_cb(ui_graphique, formaterEtiquettesAxes, LV_EVENT_DRAW_PART_BEGIN, NULL);

    // Les légendes prennent exactement la couleur de leur courbe
    lv_obj_set_style_text_color(ui_lbdistance, lv_color_hex(COULEUR_DISTANCE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbCourant,  lv_color_hex(COULEUR_COURANT),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbTension,  lv_color_hex(COULEUR_TENSION),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_lbdistance, "Distance");

    clearGraph();
}

void clearGraph() {
    nbPointsValides = 0;

    // Échelles de départ
    echelleDistance_m = ECHELLE_MIN_DISTANCE_M;
    calculerEchelleCourant(ECHELLE_MIN_COURANT_A);

    // Tension : de TENSION_BAS_V à TENSION_HAUT_V, ou jusqu'à la tension
    // de la batterie si elle est plus haute (ex. batterie 3S)
    float vBatterie = INA.getBusVoltage();
    float vHaut = (vBatterie > TENSION_HAUT_V) ? vBatterie : TENSION_HAUT_V;
    calculerEchelleTension(TENSION_BAS_V, vHaut);

    for (int i = 0; i < BUFFER_SIZE; i++) {
        tableauTemps[i] = 0;
        tableauSauvegarde[i] = 0;
        tableauDistance[i] = 0;
        tableauCourant[i] = 0;
        tableauTension[i] = 0;
        cache_serie1[i] = LV_CHART_POINT_NONE;
        cache_serie2[i] = LV_CHART_POINT_NONE;
        cache_serie3[i] = LV_CHART_POINT_NONE;
    }

    appliquerEchelles();
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

// ── MISE À JOUR DU GRAPHIQUE (CŒUR 1, appelée dans loop) ───────────────
// Toutes les fonctions LVGL sont appelées ici, jamais depuis le Cœur 0.
void updateGraph(){
    if (flagRafraichirGraphique) {
        flagRafraichirGraphique = false;

        int n = nbPointsValides;

        // 1. Min / max des mesures
        float distanceMax = 0, courantMax = 0;
        float tensionMin = tensionBas_V, tensionMax = tensionHaut_V;
        for (int i = 0; i < n; i++) {
            float d = tableauDistance[i] / 1000.0f;
            if (d > distanceMax) distanceMax = d;
            if (tableauCourant[i] > courantMax) courantMax = tableauCourant[i];
            if (tableauTension[i] < tensionMin) tensionMin = tableauTension[i];
            if (tableauTension[i] > tensionMax) tensionMax = tableauTension[i];
        }

        // 2. Les échelles ne font que s'agrandir pendant un enregistrement :
        //    vers le haut pour la distance et le courant,
        //    vers le bas (et le haut si besoin) pour la tension.
        if (distanceMax > echelleDistance_m) echelleDistance_m = distanceMax * 1.1f;
        bool echelleChangee = false;
        if (courantMax > echelleCourant_A) {
            calculerEchelleCourant(courantMax);
            echelleChangee = true;
        }
        if (tensionMin < tensionBas_V || tensionMax > tensionHaut_V) {
            calculerEchelleTension(tensionMin, tensionMax);
            echelleChangee = true;
        }
        if (echelleChangee) appliquerEchelles();

        // 3. Conversion des mesures en points du graphique
        float pleineEchelleGauche = echelleCourant_A * UNITE_COURANT;
        for (int i = 0; i < n; i++) {
            // Distance : ramenée sur la hauteur de l'axe de gauche
            float d = tableauDistance[i] / 1000.0f;
            cache_serie1[i] = (lv_coord_t)(d / echelleDistance_m * pleineEchelleGauche);

            float c = tableauCourant[i];
            if (c < 0) c = 0;   // courant négatif (shunt inversé ?) : affiché à 0
            cache_serie2[i] = (lv_coord_t)lroundf(c * UNITE_COURANT);

            cache_serie3[i] = (lv_coord_t)lroundf(tableauTension[i] * UNITE_TENSION);
        }

        lv_chart_refresh(ui_graphique);
    }

    // 4. Fin d'enregistrement (buffer plein) : mise à jour des boutons
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
    TickType_t dernierReveil = xTaskGetTickCount();

    for(;;) {

        if(enregistrementActif){

            // --- 1. Lecture des mesures ---
            uint32_t maintenant = millis();
            if (indexTableau == 0) debutEnregistrementMs = maintenant;
            uint32_t valeurActuelle = compteurCoups;

            tableauTemps[indexTableau]      = maintenant - debutEnregistrementMs;
            tableauSauvegarde[indexTableau] = valeurActuelle;
            tableauDistance[indexTableau]   = valeurActuelle * DISTANCE_INCREMENT; // mm
            tableauCourant[indexTableau]    = INA.getCurrent();                    // A
            tableauTension[indexTableau]    = INA.getBusVoltage();                 // V

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
        // --- Période fixe de 50 ms (20 Hz), indépendante du temps de lecture ---
        vTaskDelayUntil(&dernierReveil, pdMS_TO_TICKS(periodeMs));
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