# Script PlatformIO exécuté avant chaque compilation.
# Il écrit l'heure locale du PC dans include/build_time.h pour que le CoreS3
# puisse régler son horloge (RTC) au premier démarrage après un flash.
#
# Seul rtc_init.cpp inclut ce fichier : c'est le seul fichier recompilé à
# chaque build, le reste du projet (LVGL, etc.) n'est pas recompilé.

Import("env")

import os
from datetime import datetime

maintenant = datetime.now()
chemin = os.path.join(env.subst("$PROJECT_INCLUDE_DIR"), "build_time.h")

contenu = (
    "// Fichier généré automatiquement par scripts/heure_compilation.py\n"
    "// Ne pas modifier : il est réécrit à chaque compilation.\n"
    "#pragma once\n"
    f"#define BUILD_ANNEE   {maintenant.year}\n"
    f"#define BUILD_MOIS    {maintenant.month}\n"
    f"#define BUILD_JOUR    {maintenant.day}\n"
    f"#define BUILD_HEURE   {maintenant.hour}\n"
    f"#define BUILD_MINUTE  {maintenant.minute}\n"
    f"#define BUILD_SECONDE {maintenant.second}\n"
    f"#define BUILD_ID      \"{maintenant.strftime('%Y%m%d%H%M%S')}\"\n"
)

with open(chemin, "w", encoding="utf-8") as f:
    f.write(contenu)

print(f"[RTC] Heure de compilation : {maintenant.strftime('%Y-%m-%d %H:%M:%S')}")
