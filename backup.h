/**
 * @file backup.h
 * @brief API pública para el sistema de backup y restauración de base de datos
 *
 * Proporciona funcionalidades para crear copias de seguridad con marca de tiempo,
 * listar backups disponibles, restaurar desde una copia de seguridad y eliminar
 * backups antiguos. Los backups se almacenan en el directorio Exportaciones/backups/
 * con un manifiesto JSON (backups.json) que contiene los metadatos de cada backup.
 */

#ifndef BACKUP_H
#define BACKUP_H

/**
 * @brief Muestra el menú interactivo de backup y restauración
 *
 * Presenta opciones para crear, listar, restaurar y eliminar backups.
 * Gestiona el flujo de navegación del usuario dentro del submódulo.
 */
void menu_backup_restore(void);

/**
 * @brief Crea una copia de seguridad con marca de tiempo y descripción
 *
 * Genera una copia exacta del archivo de base de datos SQLite en el directorio
 * Exportaciones/backups/ con un nombre de archivo en el formato
 * YYYYMMDD_HHMMSS_descripcion.db y registra la entrada en el manifiesto JSON.
 *
 * @param descripcion Texto descriptivo para identificar el backup (opcional, puede ser NULL)
 * @return 1 si el backup se creó correctamente, 0 en caso de error
 */
int crear_backup(const char *descripcion);

/**
 * @brief Lista todas las copias de seguridad disponibles
 *
 * Lee el manifiesto backups.json y muestra en pantalla todos los backups
 * registrados con su información: nombre de archivo, descripción, fecha y tamaño.
 * Verifica que los archivos .db existan realmente en disco.
 *
 * @return 1 si hay backups listados, 0 si no hay backups o hay error
 */
int listar_backups(void);

/**
 * @brief Restaura la base de datos desde un archivo de backup
 *
 * Realiza una copia del archivo de backup seleccionado hacia la ubicación
 * actual de la base de datos (DB_PATH), reemplazando la base de datos activa.
 * La base de datos debe estar cerrada o se realiza un backup SQLite si es posible.
 *
 * @param filename Nombre del archivo de backup a restaurar (ej: "20260101_120000_pre_import.db")
 * @return 1 si la restauración fue exitosa, 0 en caso de error
 */
int restaurar_backup(const char *filename);

/**
 * @brief Elimina una copia de seguridad específica
 *
 * Borra el archivo .db del disco y remueve la entrada correspondiente del
 * manifiesto backups.json. Solicita confirmación antes de eliminar.
 *
 * @param filename Nombre del archivo de backup a eliminar (ej: "20260101_120000_pre_import.db")
 * @return 1 si el backup se eliminó correctamente, 0 en caso de error
 */
int eliminar_backup(const char *filename);

int verificar_backup_programado(void);

#endif
