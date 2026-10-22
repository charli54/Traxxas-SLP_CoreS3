#include <M5Unified.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "rtc_init.h"

// Heure de compilation, écrite par scripts/heure_compilation.py.
// Sans le script (ex. Arduino IDE), on se rabat sur __DATE__ / __TIME__.
#if __has_include("build_time.h")
#include "build_time.h"
#define HEURE_BUILD_DISPONIBLE 1
#else
#define HEURE_BUILD_DISPONIBLE 0
#define BUILD_ID __DATE__ " " __TIME__
#endif

// Le RTC contient l'heure LOCALE. On travaille en UTC côté système pour que
// localtime() (utilisé par la carte SD pour dater les fichiers) rende
// exactement l'heure du RTC, sans conversion de fuseau horaire.
static void fuseauUTC(){
    setenv("TZ", "UTC0", 1);
    tzset();
}

// Heure à écrire dans le RTC selon le mode choisi dans config.h.
// identifiant : chaîne unique pour ce réglage, mémorisée en flash, pour ne
// régler le RTC qu'une seule fois (et pas à chaque redémarrage).
static void heureDeReference(tm* t, String* identifiant){
    memset(t, 0, sizeof(tm));

#if RTC_REGLAGE_MANUEL
    t->tm_year = RTC_ANNEE - 1900;
    t->tm_mon  = RTC_MOIS - 1;
    t->tm_mday = RTC_JOUR;
    t->tm_hour = RTC_HEURE;
    t->tm_min  = RTC_MINUTE;
    t->tm_sec  = 0;
    char id[32];
    snprintf(id, sizeof(id), "M%04d%02d%02d%02d%02d",
             RTC_ANNEE, RTC_MOIS, RTC_JOUR, RTC_HEURE, RTC_MINUTE);
    *identifiant = id;
#elif HEURE_BUILD_DISPONIBLE
    t->tm_year = BUILD_ANNEE - 1900;
    t->tm_mon  = BUILD_MOIS - 1;
    t->tm_mday = BUILD_JOUR;
    t->tm_hour = BUILD_HEURE;
    t->tm_min  = BUILD_MINUTE;
    t->tm_sec  = BUILD_SECONDE + RTC_DECALAGE_SEC;  // temps de flash + démarrage
    *identifiant = BUILD_ID;
#else
    // __DATE__ = "Oct 22 2026", __TIME__ = "14:20:05"
    static const char* mois = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char nomMois[4] = {0};
    int jour, annee, h, m, s;
    sscanf(__DATE__, "%3s %d %d", nomMois, &jour, &annee);
    sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
    const char* p = strstr(mois, nomMois);
    t->tm_year = annee - 1900;
    t->tm_mon  = p ? (int)(p - mois) / 3 : 0;
    t->tm_mday = jour;
    t->tm_hour = h;
    t->tm_min  = m;
    t->tm_sec  = s + RTC_DECALAGE_SEC;
    *identifiant = BUILD_ID;
#endif

    mktime(t);  // normalise (ex. 65 secondes -> +1 minute) et calcule le jour de la semaine
}

// Recopie l'heure du RTC dans l'horloge système de l'ESP32
static void synchroniserHeureSysteme(){
    m5::rtc_datetime_t dt = M5.Rtc.getDateTime();

    tm t;
    memset(&t, 0, sizeof(tm));
    t.tm_year = dt.date.year - 1900;
    t.tm_mon  = dt.date.month - 1;
    t.tm_mday = dt.date.date;
    t.tm_hour = dt.time.hours;
    t.tm_min  = dt.time.minutes;
    t.tm_sec  = dt.time.seconds;

    timeval maintenant = { mktime(&t), 0 };
    settimeofday(&maintenant, nullptr);
}

void rtcInit(){
    fuseauUTC();

    tm reference;
    String identifiant;
    heureDeReference(&reference, &identifiant);

    // Dernier réglage appliqué, mémorisé en flash (NVS : survit aux flashs)
    Preferences prefs;
    prefs.begin("rtc", false);
    String dernierReglage = prefs.getString("reglage", "");

    bool nouveauReglage = (dernierReglage != identifiant);
    bool rtcAPerduLHeure = M5.Rtc.getVoltLow();  // pile du RTC vide / première mise sous tension

    if (nouveauReglage || rtcAPerduLHeure) {
        M5.Rtc.setDateTime(&reference);
        prefs.putString("reglage", identifiant);
    }
    prefs.end();

    synchroniserHeureSysteme();
}
