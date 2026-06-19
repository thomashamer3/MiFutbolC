/**
 * @file export_all.h
 * @brief Interfaz de Programación de Aplicaciones (API) para exportación completa de datos
 *
 * Este archivo define la API pública para el sistema de exportación completa en MiFutbolC.
 * Proporciona funciones para exportar todos los datos del sistema de manera integral,
 * generando archivos en múltiples formatos para diferentes usos y aplicaciones.
 *
 * @note Todas las funciones declaradas aquí son parte de la API pública y pueden ser
 *       llamadas desde otros módulos del sistema. La exportación completa genera
 *       una copia de seguridad integral de todos los datos disponibles.
 */

#ifndef EXPORT_ALL_H
#define EXPORT_ALL_H

/**
 * @brief Interfaz de usuario para exportación completa de datos
 *
 * Inicia un menú interactivo que sirve como punto de entrada principal para todas
 * las funciones de exportación del sistema. Este menú permite a los usuarios
 * seleccionar qué categorías de datos exportar y en qué formato.
 *
 */
void menu_exportar(void);

#endif
