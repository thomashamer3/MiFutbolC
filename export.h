/**
 * @file export.h
 * @brief Declaraciones de funciones para exportar datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones de exportación
 * que permiten guardar los datos del sistema en diferentes formatos:
 * CSV, TXT, JSON y HTML para camisetas, partidos, estadísticas y lesiones.
 */

/** @name Funciones de exportación de camisetas */
/** @{ */

/**
 * @brief Exporta la lista de camisetas a formato CSV
 *
 * Genera un archivo CSV con todos los registros de camisetas,
 * incluyendo ID, nombre y estado de sorteo.
 */
void exportar_camisetas_csv();

/**
 * @brief Exporta la lista de camisetas a formato TXT
 *
 * Genera un archivo de texto plano con la lista formateada de camisetas.
 */
void exportar_camisetas_txt();

/**
 * @brief Exporta la lista de camisetas a formato JSON
 *
 * Genera un archivo JSON con un array de objetos representando las camisetas.
 */
void exportar_camisetas_json();

/**
 * @brief Exporta la lista de camisetas a formato HTML
 *
 * Genera una página HTML con una tabla que muestra todas las camisetas.
 */
void exportar_camisetas_html();

/** @} */

/** @name Funciones de exportación de partidos */
/** @{ */

/**
 * @brief Exporta la lista de partidos a formato CSV
 *
 * Genera un archivo CSV con todos los registros de partidos,
 * incluyendo ID, cancha, fecha/hora, goles, asistencias y camiseta.
 */
void exportar_partidos_csv();

/**
 * @brief Exporta la lista de partidos a formato TXT
 *
 * Genera un archivo de texto plano con la lista formateada de partidos.
 */
void exportar_partidos_txt();

/**
 * @brief Exporta la lista de partidos a formato JSON
 *
 * Genera un archivo JSON con un array de objetos representando los partidos.
 */
void exportar_partidos_json();

/**
 * @brief Exporta la lista de partidos a formato HTML
 *
 * Genera una página HTML con una tabla que muestra todos los partidos.
 */
void exportar_partidos_html();

/** @} */

/** @name Funciones de exportación de estadísticas */
/** @{ */

/**
 * @brief Exporta las estadísticas generales a formato CSV
 *
 * Genera un archivo CSV con las estadísticas agregadas del sistema.
 */
void exportar_estadisticas_csv();

/**
 * @brief Exporta las estadísticas generales a formato TXT
 *
 * Genera un archivo de texto con las estadísticas agregadas del sistema.
 */
void exportar_estadisticas_txt();

/**
 * @brief Exporta las estadísticas generales a formato JSON
 *
 * Genera un archivo JSON con un objeto conteniendo todas las estadísticas.
 */
void exportar_estadisticas_json();

/**
 * @brief Exporta las estadísticas generales a formato HTML
 *
 * Genera una página HTML con las estadísticas presentadas en formato web.
 */
void exportar_estadisticas_html();

/** @} */

/** @name Funciones de exportación de lesiones */
/** @{ */

/**
 * @brief Exporta la lista de lesiones a formato CSV
 *
 * Genera un archivo CSV con todos los registros de lesiones,
 * incluyendo ID, jugador, tipo, fecha y duración.
 */
void exportar_lesiones_csv();

/**
 * @brief Exporta la lista de lesiones a formato TXT
 *
 * Genera un archivo de texto plano con la lista formateada de lesiones.
 */
void exportar_lesiones_txt();

/**
 * @brief Exporta la lista de lesiones a formato JSON
 *
 * Genera un archivo JSON con un array de objetos representando las lesiones.
 */
void exportar_lesiones_json();

/**
 * @brief Exporta la lista de lesiones a formato HTML
 *
 * Genera una página HTML con una tabla que muestra todas las lesiones.
 */
void exportar_lesiones_html();

/** @} */
