/**
 * @file ascii_art.h
 * @brief Definiciones de arte ASCII para el proyecto MiFutbolC
 */

#ifndef ASCII_ART_H
#define ASCII_ART_H

#include <stdio.h>

/**
 * @brief Arte ASCII para la pantalla de bienvenida
 */
#define ASCII_BIENVENIDA \
     ".___  ___.  __   _______  __    __  .___________..______     ______    __        ______ \n" \
    "|   \\/   | |  | |   ____||  |  |  | |           ||   _  \\   /  __  \\  |  |      /      |\n" \
    "|  \\  /  | |  | |  |__   |  |  |  | `---|  |----`|  |_)  | |  |  |  | |  |     |  ,----'\n" \
    "|  |\\/|  | |  | |   __|  |  |  |  |     |  |     |   _  <  |  |  |  | |  |     |  |     \n" \
    "|  |  |  | |  | |  |     |  `--'  |     |  |     |  |_)  | |  `--'  | |  `----.|  `----.\n" \
    "|__|  |__| |__| |__|      \\______/      |__|     |______/   \\______/  |_______| \\______|\n" \
    "                    Sistema de Gestion Futbolistica\n" \
    "                          MiFutbolC v3.0\n\n"

/**
 * @brief Arte ASCII para exportaciones exitosas
 */
#define ASCII_EXPORT_EXITOSO \
    "╔══════════════════════════════════════════════════════════════╗\n" \
    "║                    EXPORTACION EXITOSA                      ║\n" \
    "║                                                              ║\n" \
    "║  ✓ Archivos generados correctamente                          ║\n" \
    "║  ✓ Datos exportados en multiples formatos                   ║\n" \
    "║  ✓ Listos para analisis y reportes                          ║\n" \
    "╚══════════════════════════════════════════════════════════════╝\n"

/**
 * @brief Arte ASCII para camisetas
 */
#define ASCII_CAMISETA \
    "   ____                _          _            \n" \
    "  / ___|__ _ _ __ ___ (_)___  ___| |_ __ _ ___ \n" \
    " | |   / _` | '_ ` _ \\| / __|/ _ \\ __/ _` / __|\n" \
    " | |__| (_| | | | | | | \\__ \\  __/ || (_| \\__ \\\n" \
    "  \\____\\__,_|_| |_| |_|_|___/\\___|\\__\\__,_|___/\n" \
    "                                                \n"

/**
 * @brief Arte ASCII para estadisticas
 */
#define ASCII_ESTADISTICAS \
    "  _____     _            _ _     _   _               \n" \
    " | ____|___| |_ __ _  __| (_)___| |_(_) ___ __ _ ___ \n" \
    " |  _| / __| __/ _` |/ _` | / __| __| |/ __/ _` / __|\n" \
    " | |___\\__ \\ || (_| | (_| | \\__ \\ |_| | (_| (_| \\__ \\\n" \
    " |_____|___/\\__\\__,_|\\__,_|_|___/\\__|_|\\___\\__,_|___/\n"

/**
 * @brief Arte ASCII para futbol
 */
#define ASCII_FUTBOL \
    "  ____            _   _     _           \n" \
    " |  _ \\ __ _ _ __| |_(_) __| | ___  ___ \n" \
    " | |_) / _` | '__| __| |/ _` |/ _ \\/ __|\n" \
    " |  __/ (_| | |  | |_| | (_| | (_) \\__ \\\n" \
    " |_|   \\__,_|_|   \\__|_|\\__,_|\\___/|___/\n" \
    "                                        \n"

/**
 * @brief Arte ASCII para canchas
 */
#define ASCII_CANCHA \
    "   ____                 _               \n" \
    "  / ___|__ _ _ __   ___| |__   __ _ ___ \n" \
    " | |   / _` | '_ \\ / __| '_ \\ / _` / __|\n" \
    " | |__| (_| | | | | (__| | | | (_| \\__ \\\n" \
    "  \\____\\__,_|_| |_|\\___|_| |_|\\__,_|___/\n" \
    "                                        \n"

/**
 * @brief Arte ASCII para equipos
 */
#define ASCII_EQUIPO \
    "  _____            _                 \n" \
    " | ____|__ _ _   _(_)_ __   ___  ___ \n" \
    " |  _| / _` | | | | | '_ \\ / _ \\/ __|\n" \
    " | |__| (_| | |_| | | |_) | (_) \\__ \\\n" \
    " |_____\\__, |\\__,_|_| .__/ \\___/|___/\n" \
    "         |_|       |_|              \n"

/**
 * @brief Arte ASCII para logros
 */
#define ASCII_LOGROS \
    "  _                              \n" \
    " | |    ___   __ _ _ __ ___  ___ \n" \
    " | |   / _ \\ / _` | '__/ _ \\/ __|\n" \
    " | |__| (_) | (_| | | | (_) \\__ \\\n" \
    " |_____\\___/ \\__, |_|  \\___/|___/\n" \
    "             |___/               \n"

/**
 * @brief Arte ASCII para analisis
 */
#define ASCII_ANALISIS \
    "     _                _ _     _     \n" \
    "    / \\   _ __   __ _| (_)___(_)___ \n" \
    "   / _ \\ | '_ \\ / _` | | / __| / __|\n" \
    "  / ___ \\| | | | (_| | | \\__ \\ \\__ \\\n" \
    " /_/   \\_\\_| |_|\\__,_|_|_|___/_|___/\n" \
    "                                    \n"

/**
 * @brief Arte ASCII para lesiones
 */
#define ASCII_LESIONES \
    "  _              _                       \n" \
    " | |    ___  ___(_) ___  _ __   ___  ___ \n" \
    " | |   / _ \\/ __| |/ _ \\| '_ \\ / _ \\/ __|\n" \
    " | |__|  __/\\__ \\ | (_) | | | |  __/\\__ \\\n" \
    " |_____\\___||___/_|\\___/|_| |_|\\___||___/\n"

/**
 * @brief Arte ASCII para financiamiento
 */
#define ASCII_FINANCIAMIENTO \
    "  _____ _                        _                 _            _        \n" \
    " |  ___(_)_ __   __ _ _ __   ___(_) __ _ _ __ ___ (_) ___ _ __ | |_ ___  \n" \
    " | |_  | | '_ \\ / _` | '_ \\ / __| |/ _` | '_ ` _ \\| |/ _ \\ '_ \\| __/ _ \\ \n" \
    " |  _| | | | | | (_| | | | | (__| | (_| | | | | | | |  __/ | | | || (_) |\n" \
    " |_|   |_|_| |_|\\__,_|_| |_|\\___|_|\\__,_|_| |_| |_|_|\\___|_| |_|\\__|\\___/ \n"

/**
 * @brief Arte ASCII para exportar
 */
#define ASCII_EXPORTAR \
    "  _____                       _             \n" \
    " | ____|_  ___ __   ___  _ __| |_ __ _ _ __ \n" \
    " |  _| \\ \\/ / '_ \\ / _ \\| '__| __/ _` | '__|\n" \
    " | |___ >  <| |_) | (_) | |  | || (_| | |   \n" \
    " |_____/_/\\_\\ .__/ \\___/|_|   \\__\\__,_|_|   \n" \
    "            |_|                              \n"

/**
 * @brief Arte ASCII para importar
 */
#define ASCII_IMPORTAR \
    "  ___                            _             \n" \
    " |_ _|_ __ ___  _ __   ___  _ __| |_ __ _ _ __ \n" \
    "  | || '_ ` _ \\| '_ \\ / _ \\| '__| __/ _` | '__|\n" \
    "  | || | | | | | |_) | (_) | |  | || (_| | |   \n" \
    " |___|_| |_| |_| .__/ \\___/|_|   \\__\\__,_|_|   \n" \
    "               |_|                              \n"

/**
 * @brief Arte ASCII para torneos
 */
#define ASCII_TORNEOS \
    "  _____                               \n" \
    " |_   _|__  _ __ _ __   ___  ___  ___ \n" \
    "   | |/ _ \\| '__| '_ \\ / _ \\/ _ \\/ __|\n" \
    "   | | (_) | |  | | | |  __/ (_) \\__ \\\n" \
    "   |_|\\___/|_|  |_| |_|\\___|\\___/|___/\n"

/**
 * @brief Arte ASCII para ajustes
 */
#define ASCII_AJUSTES \
    "     _     _           _            \n" \
    "    / \\   (_)_   _ ___| |_ ___  ___ \n" \
    "   / _ \\  | | | | / __| __/ _ \\/ __|\n" \
    "  / ___ \\ | | |_| \\__ \\ ||  __/\\__ \\\n" \
    " /_/   \\_\\/ |\\__,_|___/\\__\\___||___/\n" \
    "        |__/                        \n"

/**
 * @brief Genera cancha de fútbol animada con balón en movimiento
 * @param minuto Minuto actual del partido
 * @param evento_tipo Tipo de evento (0=normal, 1=gol, 2=oportunidad, 3=falta)
 */
void mostrar_cancha_animada(int minuto, int evento_tipo);

/**
 * @brief Arte ASCII para título de simulación de partido
 */
#define ASCII_SIMULACION \
    ".___  ___.  __   _______  __    __  .___________..______     ______    __        ______ \n" \
    "|   \\/   | |  | |   ____||  |  |  | |           ||   _  \\   /  __  \\  |  |      /      |\n" \
    "|  \\  /  | |  | |  |__   |  |  |  | `---|  |----`|  |_)  | |  |  |  | |  |     |  ,----'\n" \
    "|  |\\/|  | |  | |   __|  |  |  |  |     |  |     |   _  <  |  |  |  | |  |     |  |     \n" \
    "|  |  |  | |  | |  |     |  `--'  |     |  |     |  |_)  | |  `--'  | |  `----.|  `----.\n" \
    "|__|  |__| |__| |__|      \\______/      |__|     |______/   \\______/  |_______| \\______|\n"

#endif /* ASCII_ART_H */
