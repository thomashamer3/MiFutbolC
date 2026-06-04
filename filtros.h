#ifndef FILTROS_H
#define FILTROS_H

typedef struct
{
    int usar_fecha_desde;
    int usar_fecha_hasta;
    int usar_resultado;
    int usar_equipo;
    int usar_formacion;
    int usar_competicion;

    char fecha_desde[32];
    char fecha_hasta[32];
    int resultado;
    int equipo_id;
    char formacion[16];
    int competicion_id;
} FiltrosBusqueda;

void menu_filtros_avanzados(void);
int aplicar_filtros_partidos(FiltrosBusqueda *f);

#endif
