/**
 * @file clima_diferido.h
 * @brief Clima de partidos completado en segundo plano.
 *
 * Crear un partido no espera a Open-Meteo: el partido queda en la tabla
 * partido_clima_pendiente y el clima se pide con un proceso curl aparte
 * (sin hilos: SQLite esta compilado con SQLITE_THREADSAFE=0). El hilo
 * principal llama a clima_diferido_procesar() en cada vuelta de menu para
 * revisar el resultado sin bloquear.
 */

#ifndef CLIMA_DIFERIDO_H
#define CLIMA_DIFERIDO_H

/** @brief Crea la tabla de pendientes y su trigger de borrado si faltan. */
int clima_diferido_init(void);

/**
 * @brief Deja el clima del partido pendiente (reinicia los intentos si ya estaba).
 * @return 1 si quedo encolado, 0 si fallo.
 */
int clima_diferido_encolar(long long partido_id);

/** @brief 1 si la cancha existe y tiene latitud/longitud distintas de 0,0. */
int clima_diferido_cancha_tiene_coordenadas(int cancha_id);

/**
 * @brief Sondeo no bloqueante: aplica un resultado terminado y lanza el
 *        siguiente pendiente. Como maximo un curl a la vez; no imprime nada.
 *        No hace nada en MODE_SIMPLE.
 */
void clima_diferido_procesar(void);

/** @brief Corta el curl en curso y borra su archivo; la fila sigue pendiente. */
void clima_diferido_finalizar(void);

#ifdef UNIT_TEST
/* Ganchos de prueba: exponen la logica interna sin lanzar curl. */
int clima_diferido_test_ya_intentado(long long partido_id);
void clima_diferido_test_marcar_intentado(long long partido_id);
int clima_diferido_test_intentados_cant(void);
void clima_diferido_test_registrar_fallo(long long partido_id, int anio, int mes, int dia);
/** @brief Siguiente pendiente lanzable (0 si no hay); aplica los descartes. */
long long clima_diferido_test_buscar_siguiente(void);
/**
 * @brief Aplica una respuesta ya descargada en @p ruta como si la hubiera
 *        bajado curl para ese partido, cancha y fecha (la del lanzamiento).
 */
void clima_diferido_test_aplicar_archivo(long long partido_id, int cancha_id,
        const char *fecha_hora, const char *ruta, int codigo_salida);
#endif

#endif
