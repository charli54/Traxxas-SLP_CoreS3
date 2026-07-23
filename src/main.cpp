#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>

#include "ui/ui.h"
#include "screen_init.h"
#include "SD_init.h"
#include "tusb.h"
#include "config.h"
#include "record.h"
#include "INA226_init.h"

bool lastUSBStateConnected = true;
bool lastUsbPretEtMonte = false;
bool lastUsbMountedState = false;
static String listeSD = "";
//------------------------------ENCODEUR----------------------------------------


void setup() {
  // put your setup code here, to run once:
  screen_init();
  ui_init();

  if(!sdInit()){
    lv_obj_add_state(ui_btSauvegarde, LV_STATE_DISABLED);
  }

  disableUsbMode();

  init_INA226();
  initInterruptmagneticSwitch();
  initThreadCalculEncodeur();

  clearGraph();

}

void loop() {
  M5.update();
  lv_timer_handler();

  updateGraph();
  
/*
  int soc = M5.Power.getBatteryLevel();
  int bat_vol = M5.Power.getBatteryVoltage();
  lv_label_set_text_fmt(ui_lbBatterieSOC, "%d %%", soc);
*/

    // 1. On lit l'état actuel directement
    bool USBConnected = isUSBReady();

    /*if (usbPretEtMonte != lastUsbPretEtMonte){
      lastUsbPretEtMonte = usbPretEtMonte;

      if(usbPretEtMonte){
        lv_label_set_text(ui_lbBatterieSOC, "READY");
      }
      else{
        lv_label_set_text(ui_lbBatterieSOC, "NO READY");
      }
    }*/

    // 2. Le Latch : On vérifie s'il y a eu un CHANGEMENT d'état
    if (USBConnected != lastUSBStateConnected) {
        lastUSBStateConnected = USBConnected; // On mémorise le nouvel état immédiatement

        // 3. On applique l'action graphique une seule fois selon le cas
        if (USBConnected) {
            lv_obj_clear_state(ui_btTransfert, LV_STATE_DISABLED);
            lv_obj_clear_state(ui_btTransfertScr4, LV_STATE_DISABLED);

        } else {
            lv_obj_add_state(ui_btTransfert, LV_STATE_DISABLED);
            lv_obj_add_state(ui_btTransfertScr4, LV_STATE_DISABLED);
        }
        // Rafraichissement de l'Ecran
        lv_obj_invalidate(ui_btTransfert);
        lv_obj_invalidate(ui_btTransfertScr4);
    }

    if (isUsbModeActive() && usbEjected){

      disableUsbMode();

      // 2. CORRECTION GRAPHISME : On retire l'état "coché" pour couper le bouton
      lv_obj_clear_state(ui_btTransfert, LV_STATE_CHECKED);
      lv_obj_clear_state(ui_btTransfertScr4, LV_STATE_CHECKED);
      // 3. (Optionnel mais recommandé) Si le bouton était resté en état "pressé" ou "focus"
      // on peut nettoyer pour être sûr qu'il reprenne son image RELEASED standard
      lv_obj_clear_state(ui_btTransfert, LV_STATE_FOCUSED | LV_STATE_PRESSED);
      lv_obj_clear_state(ui_btTransfertScr4, LV_STATE_FOCUSED | LV_STATE_PRESSED);
      lv_obj_invalidate(ui_btTransfert);
      lv_obj_invalidate(ui_btTransfertScr4);

    }
  delay(5);
}
