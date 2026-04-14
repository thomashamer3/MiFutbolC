/**
 * @file atajos.h
 * @brief Sistema de atajos de teclado para MiFutbolC
 *
 * Permite ejecutar acciones comunes mediante combinaciones de teclas.
 * Compatible con Windows y Unix/Linux.
 */

#ifndef ATAJOS_H
#define ATAJOS_H

/**
 * @brief Inicializa el sistema de atajos de teclado
 *
 * Configura el modo de entrada para captura de combinaciones.
 */
void inicializar_atajos();

/**
 * @brief Verifica si hay un atajo de teclado presionado
 *
 * @return 1 si se detectó un atajo, 0 en caso contrario
 */
int verificar_atajo();

/**
 * @brief Muestra la ayuda de atajos disponibles
 */
void mostrar_ayuda_atajos();

/**
 * @brief Finaliza el sistema de atajos
 *
 * Restaura el modo de terminal normal.
 */
void finalizar_atajos();

// Códigos de atajos
#define ATAJO_DASHBOARD   'D'
#define ATAJO_BUSQUEDA    'B'
#define ATAJO_CALENDARIO  'C'
#define ATAJO_NUEVO_PARTIDO 'N'
#define ATAJO_ESTADISTICAS  'S'
#define ATAJO_AYUDA       'H'
#define ATAJO_SALIR       'Q'

#endif // ATAJOS_H
