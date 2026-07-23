#include <INA226.h>
#include "config.h"
#include "INA226_init.h"

INA226 INA(INA226_ADDR);

void init_INA226(){
    Wire.begin(SDA_PIN, SCL_PIN);

    // Initialisation du capteur INA226
    if (!INA.begin()) {
        while (1) {
            delay(1000);
        }
    }

    // Configuration pour un shunt de 1 mOhm (0.001 Ohm)
    // Paramètres : setMaxCurrentShunt(MaxCurrent_In_Amperes, Shunt_Resistor_In_Ohms)
    // Avec un shunt de 1 mOhm, la tension de shunt max autorisée (81.92 mV) donne un courant max de ~81.9 A
    INA.setMaxCurrentShunt(INA_MAX_CURRENT, INA_SHUNT_RESISTOR);
    
}