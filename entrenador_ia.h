#ifndef ENTRENADOR_IA_H
#define ENTRENADOR_IA_H

#include <time.h>

// Estado del jugador evaluado
typedef struct
{
    float rendimiento_promedio;
    float cansancio_promedio;
    float estado_animo_promedio;
    int partidos_consecutivos;
    float riesgo_lesion;
    int derrotas_consecutivas;
    int dias_descanso;
} EstadoJugador;

// Niveles de importancia de los consejos
typedef enum
{
    CONSEJO_INFO,
    CONSEJO_ADVERTENCIA,
    CONSEJO_CRITICO
} NivelConsejo;

// Categorías de consejos
typedef enum
{
    CATEGORIA_FISICO,
    CATEGORIA_MENTAL,
    CATEGORIA_DEPORTIVO,
    CATEGORIA_SALUD,
    CATEGORIA_GESTION
} CategoriaConsejo;

// Estructura para un consejo
typedef struct
{
    char *mensaje;
    NivelConsejo nivel;
    CategoriaConsejo categoria;
} Consejo;

// Historial de consejos
typedef struct
{
    time_t fecha;
    char *consejo;
    int seguido; // 1 si el usuario lo siguió, 0 si no
} HistorialConsejo;

// Perfil del usuario (bonus)
typedef struct
{
    int consejos_aceptados;
    int consejos_ignorados;
    float indice_prudencia; // 0-1, cuanto más alto más prudente
} PerfilUsuarioIA;

// Funciones principales
EstadoJugador evaluar_estado_jugador();
void generar_consejos(EstadoJugador estado, Consejo **consejos, int *num_consejos);
void mostrar_consejos_actuales();
void mostrar_historial_consejos();
void evaluar_decision_pasada();
void configurar_nivel_intervencion();
void guardar_consejo_historial(const char *consejo, int seguido);
PerfilUsuarioIA obtener_perfil_usuario();
void actualizar_perfil_usuario(int consejo_seguido);

// Funciones auxiliares
const char* nivel_a_string(NivelConsejo nivel);
const char* categoria_a_string(CategoriaConsejo categoria);

// Triggers de activación
void activar_ia_antes_partido();
void activar_ia_antes_torneo();
void activar_ia_estadisticas();

// Menú principal de la IA
void menu_entrenador_ia();

#endif
