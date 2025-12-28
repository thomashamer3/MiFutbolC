/**
 * @file export_all.h
 * @brief Declaraciones de funciones para exportación completa en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones que permiten
 * exportar todos los datos del sistema de una sola vez en múltiples formatos.
 */

/**
 * @brief Menu para exportar todos los datos del sistema en todos los formatos disponibles
 *
 * Esta función ejecuta todas las funciones de exportación para camisetas,
 * partidos, estadísticas y lesiones, generando archivos en formatos
 * CSV, TXT, JSON y HTML para cada tipo de dato.
 *
 * @note Los archivos se generan en el directorio 'data/' del proyecto
 * @note Esta operación puede tomar tiempo dependiendo de la cantidad de datos
 */
void menu_exportar();
