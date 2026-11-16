/**
 * @file partido.h
 * @brief Declaraciones de funciones para la gestión de partidos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de partidos de fútbol, incluyendo creación, listado, modificación
 * y eliminación de registros de partidos.
 */

#ifndef PARTIDO_H
#define PARTIDO_H

/**
 * @brief Muestra el menú principal de gestión de partidos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar partidos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos(void);

/**
 * @brief Crea un nuevo partido en la base de datos
 *
 * Solicita al usuario los datos del partido (cancha, goles, asistencias, camiseta)
 * y lo inserta en la tabla 'partido'. Obtiene la fecha y hora actual automáticamente.
 * Verifica que la camiseta seleccionada exista antes de insertar.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_partido(void);

/**
 * @brief Muestra un listado de todos los partidos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los partidos
 * con sus respectivos datos: ID, cancha, fecha/hora, goles, asistencias
 * y nombre de la camiseta utilizada. Realiza un JOIN con la tabla camiseta
 * para obtener el nombre de la camiseta.
 *
 * @note Si no hay partidos registrados, muestra un mensaje informativo
 */
void listar_partidos(void);

/**
 * @brief Permite eliminar un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de proceder con
 * la eliminación del registro de la base de datos.
 */
void eliminar_partido(void);

/**
 * @brief Permite modificar los datos de un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar todos los campos del partido:
 * cancha, goles, asistencias y camiseta utilizada.
 * Verifica que la camiseta especificada exista antes de actualizar.
 */
void modificar_partido(void);

/**
 * @brief Permite buscar partidos según diferentes criterios
 *
 * Presenta un submenú con opciones para buscar partidos por:
 * - Camiseta utilizada
 * - Número de goles
 * - Número de asistencias
 * - Cancha donde se jugó
 */
void buscar_partidos(void);

/**
 * @brief Obtiene datos climáticos para partidos históricos sin temperatura
 *
 * Recorre todos los partidos con temperatura_c IS NULL y cuya cancha tenga
 * coordenadas, consulta la API Open-Meteo y guarda los datos en la DB.
 */
void obtener_clima_partidos_historicos(void);

/**
 * @brief Interpreta la fecha_hora de un partido ("AAAA-MM-DD HH:MM" o "DD/MM/AAAA").
 *
 * Sin hora explicita deja 12:00.
 *
 * @return 1 si la fecha es valida, 0 si no.
 */
int partido_parsear_fecha_hora(const char *fecha, int *anio, int *mes, int *dia, int *hora,
                               int *minuto);

/**
 * @brief Devuelve el id para un partido nuevo.
 *
 * Los IDs de partido son estables (no se renumeran por fecha) y nunca se
 * reutilizan: el valor es mayor que cualquier id existente o borrado, para que
 * un partido nuevo no herede referencias de uno eliminado.
 */
long long partido_siguiente_id(void);

/**
 * @brief Limpia las filas que dependen de los partidos donde @p columna = @p valor.
 *
 * Borra las filas que son del partido (partido_meta, partido_tag,
 * carrera_partido_hito, quimica_jugador_estadistica) y desvincula las
 * entidades propias (lesion y bienestar_sesion_mental a NULL, media a 0,
 * equipo a -1). Las tablas que no existen se saltean. Debe llamarse antes del
 * DELETE de partido y en la misma transaccion.
 *
 * @param columna "id", "cancha_id" o "camiseta_id"; cualquier otra se rechaza.
 * @return 1 si todo se aplico, 0 ante error.
 */
int partido_borrar_dependencias_por(const char *columna, long long valor);

/** @brief Igual que partido_borrar_dependencias_por("id", partido_id). */
int partido_borrar_dependencias(long long partido_id);

/**
 * @brief Borra un partido y sus dependencias en una transaccion (SAVEPOINT).
 * @return 1 si el partido existia y se borro, 0 si no.
 */
int partido_eliminar_por_id(long long partido_id);

/**
 * @brief Muestra el submenu de ultimos partidos
 *
 * Presenta un menu con opciones para ver:
 * - Ultimo partido jugado
 * - Ultima victoria
 * - Ultima derrota
 * - Ultimo empate
 * - Ultimo partido con goles
 * - Ultimo partido con asistencias
 */
void menu_ultimo(void);

#endif
