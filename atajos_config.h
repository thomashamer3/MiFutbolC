/**
 * @file atajos_config.h
 * @brief API publica para configuracion de atajos de teclado
 *
 * Permite al usuario personalizar las teclas de acceso rapido
 * a las diferentes funcionalidades del sistema.
 */

#ifndef ATAJOS_CONFIG_H
#define ATAJOS_CONFIG_H

/**
 * @brief Muestra el menu de configuracion de atajos
 */
void menu_atajos_config(void);

/**
 * @brief Lista todos los atajos configurados
 */
void listar_atajos_config(void);

/**
 * @brief Modifica un atajo existente
 */
void modificar_atajo_config(void);

/**
 * @brief Restaura todos los atajos a los valores por defecto
 */
void restaurar_atajos_default(void);

/**
 * @brief Obiene la tecla configurada para una accion
 * @param accion Nombre de la accion a buscar
 * @return Caracter de la tecla asignada, o 0 si no existe
 */
char obtener_atajo_para(const char *accion);

#endif
