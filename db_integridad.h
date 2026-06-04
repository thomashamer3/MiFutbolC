/**
 * @file db_integridad.h
 * @brief Verificacion y reparacion de integridad de la base de datos SQLite
 *
 * Proporciona funciones para comprobar la integridad fisica y logica de la
 * base de datos, reconstruir archivos, actualizar estadisticas del optimizador
 * de consultas y generar informacion detallada sobre el estado de la base de datos.
 */

#ifndef DB_INTEGRIDAD_H
#define DB_INTEGRIDAD_H

/**
 * @brief Ejecuta PRAGMA integrity_check y muestra resultados detallados
 *
 * Realiza una verificacion completa de integridad de la base de datos,
 * revisando estructuras internas, indices y referencias.
 */
void verificar_integridad_bd(void);

/**
 * @brief Ejecuta PRAGMA quick_check (verificacion rapida de integridad)
 *
 * Version optimizada de integrity_check que realiza una verificacion
 * menos exhaustiva pero mas rapida.
 */
void verificar_integrdiad_rapida(void);

/**
 * @brief Reconstruye la base de datos para recuperar espacio (VACUUM)
 *
 * Ejecuta VACUUM para reconstruir el archivo de base de datos completo,
 * recuperando espacio no utilizado. Puede ser lento en BD grandes.
 * @note Se recomienda realizar un backup antes de esta operacion
 */
void reconstruir_bd(void);

/**
 * @brief Analiza tablas para actualizar estadisticas del optimizador de consultas
 *
 * Ejecuta ANALYZE para recopilar estadisticas sobre tablas e indices,
 * permitiendo al optimizador de SQLite generar planes mas eficientes.
 */
void analizar_bd(void);

/**
 * @brief Verifica y repara problemas comunes de integridad
 *
 * Ejecuta integrity_check y, si se encuentran errores, intenta
 * repararlos mediante REINDEX y VACUUM.
 */
void reparar_bd(void);

/**
 * @brief Muestra informacion detallada del estado de la base de datos
 *
 * Consulta y muestra: cantidad de paginas, tamano de pagina,
 * version del esquema, paginas libres y version de usuario.
 */
void mostrar_info_bd(void);

/**
 * @brief Menu interactivo para todas las operaciones de integridad
 */
void menu_integridad_bd(void);

#endif /* DB_INTEGRIDAD_H */
