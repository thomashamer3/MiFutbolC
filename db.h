/**
 * @file db.h
 * @brief API de persistencia de datos relacional con SQLite3
 *
 * Define interfaz para operaciones de base de datos utilizando motor SQLite3,
 * implementando patrón Singleton para conexión global, configuración automática
 * de esquema relacional y gestión de directorios específicos del sistema operativo.
 * Soporta evolución de esquema mediante ALTER TABLE dinámicos.
 */

#ifndef DB_H
#define DB_H

#include "compat_port.h"
#include "sqlite3.h"

/**
 * @brief Instancia global de conexión SQLite3
 *
 * Puntero singleton que mantiene estado de conexión activa durante
 * ciclo de vida de la aplicación, utilizado por todas las operaciones
 * de consulta y modificación de datos.
 */
extern sqlite3 *db;

/**
 * @brief Crea un backup automático de la base de datos
 *
 * Esta función genera una copia con timestamp en la carpeta de exportaciones
 * para prevenir pérdida de datos antes de importaciones.
 *
 * @param motivo Texto opcional para identificar el contexto del backup.
 * @return 1 si se creó el backup, 0 si falló.
 */
int backup_base_datos_automatico(const char *motivo);

/**
 * @brief Inicializa infraestructura completa de persistencia
 *
 * Ejecuta secuencia de configuración: rutas del SO, conexión SQLite,
 * creación de esquema relacional completo, adición de columnas faltantes
 * y preparación de directorios auxiliares para import/export.
 *
 * @return 1 si configuración completa exitosa, 0 en caso de error crítico
 */
int db_init();

/**
 * @brief Finaliza conexión y libera recursos del motor SQLite
 *
 * Cierra handle de base de datos de manera ordenada, asegurando
 * commit de transacciones pendientes y liberación de memoria.
 */
void db_close();

/**
 * @brief Recupera configuración de usuario desde tabla relacional
 *
 * Ejecuta consulta SELECT preparada sobre tabla 'usuario' con
 * límite de resultado único, retornando configuración personalizada.
 *
 * @return Puntero dinámico a string con nombre de usuario, NULL si no existe
 */
char* get_user_name();

/**
 * @brief Persiste configuración de usuario en base de datos
 *
 * Utiliza INSERT OR REPLACE para upsert de configuración personal,
 * permitiendo actualización atómica de preferencias del usuario.
 *
 * @param nombre String con identificador de usuario a almacenar
 * @return 1 si operación de escritura exitosa, 0 en caso de error de BD
 */
int set_user_name(const char* nombre);

/**
 * @brief Proporciona ruta absoluta al directorio de datos internos
 *
 * Retorna ubicación del sistema de archivos donde se almacenan
 * archivos persistentes de la aplicación (base de datos, configuración).
 *
 * @return Puntero constante a string con path del directorio de datos
 */
const char* get_data_dir();

/**
 * @brief Determina ubicación canónica para archivos de exportación
 *
 * Configura directorio visible al usuario (Documents en Windows)
 * para almacenamiento de datos exportados en formatos externos,
 * facilitando backup y portabilidad de información.
 *
 * @return Puntero constante a string con path del directorio de exportaciones
 */
const char* get_export_dir();

/**
 * @brief Determina ubicación canónica para archivos de importación
 *
 * Configura directorio accesible al usuario para colocación de
 * archivos fuente de datos, permitiendo carga masiva desde
 * formatos externos mediante operaciones de importación.
 *
 * @return Puntero constante a string con path del directorio de importaciones
 */
const char* get_import_dir();

/**
 * @brief Registra un evento informativo en el archivo de log de la aplicación
 *
 * Permite registrar acciones de usuario y eventos de flujo funcional
 * desde cualquier módulo de la aplicación.
 *
 * @param component Componente o módulo emisor (ej: "MENU", "CAMISETA")
 * @param message Mensaje del evento
 */
void app_log_event(const char *component, const char *message);

/**
 * @brief Copia la base de datos SQLite a la carpeta de documentos
 *
 * Esta función realiza una copia exacta del archivo de base de datos
 * desde la ubicación de datos internos a la carpeta de exportación
 * (normalmente Documentos del usuario).
 */
void exportar_base_datos();

/**
 * @brief Importa una base de datos SQLite desde un archivo externo
 *
 * Esta función permite al usuario importar una base de datos completa
 * desde un archivo externo, reemplazando la base de datos actual.
 * Se realiza una copia exacta del archivo de origen al directorio de datos.
 */
void importar_base_datos();

#endif
