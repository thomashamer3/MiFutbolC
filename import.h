/**
 * @file import.h
 * @brief Declaraciones de funciones para importar datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones de importación
 * que permiten cargar los datos del sistema desde archivos JSON.
 */

/** @name Funciones de importación de camisetas */
/** @{ */

/**
 * @brief Importa camisetas desde archivo JSON
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json();

/** @} */

/** @name Funciones de importación de partidos */
/** @{ */

/**
 * @brief Importa partidos desde archivo JSON
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json();

/**
 * @brief Importa lesiones desde archivo JSON
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json();

/**
 * @brief Importa estadísticas desde archivo JSON
 *
 * Lee el archivo JSON de estadísticas y las procesa.
 */
void importar_estadisticas_json();

/** @} */

void menu_importar();
