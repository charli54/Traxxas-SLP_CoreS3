#ifndef RTC_INIT_H
#define RTC_INIT_H

// Règle le RTC du CoreS3 si besoin (nouveau flash, réglage manuel dans
// config.h ou RTC qui a perdu l'heure), puis recopie l'heure du RTC dans
// l'horloge système. La carte SD utilise l'horloge système pour dater
// les fichiers créés.
// À appeler après M5.begin() (donc après screen_init()).
void rtcInit(void);

#endif // RTC_INIT_H
