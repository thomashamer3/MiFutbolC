# Changelog

Todos los cambios importantes de MiFutbolC se anotan en este archivo.
El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La última versión publicada es la 4.6.0.

## [Sin publicar]

### Agregado
- **Modificar Partido muestra los datos actuales.** Al elegir el partido aparece una ficha con cancha, fecha, goles, asistencias, resultado, diferencia de gol, camiseta, clima, franja, precio y comentario. Cada campo muestra su valor actual ("Actual: X") antes de pedir el nuevo.
- **Eliminar Partido ofrece borrar el gasto asociado.** Si el partido tiene un gasto de cancha en Finanzas, se pregunta si también se borra (por defecto se conserva). Las dos cosas se borran juntas o no se borra ninguna.
- **Tests de Modificar Partido** (`tests/test_partido_modificar.c`, 14 casos). Cubren la diferencia de gol, el guardado con chequeo, la sincronización con Finanzas, el reencolado del clima y la eliminación con gasto. También recorren el menú de Modificar Partido de punta a punta, con entradas simuladas.

### Cambiado
- **Fecha y hora al modificar:** se valida el formato (`dd/mm/aaaa hh:mm`), Enter usa la fecha y hora actual, y la franja horaria se recalcula sola.
- **"Modificar Todo" pasa a llamarse "Modificar Todo (datos principales)".** Ahora pide la diferencia de gol, valida los rangos y calcula la franja desde la hora en vez de preguntarla.
- **Cambiar el resultado** ajusta el signo de la diferencia de gol: positiva en una victoria, negativa en una derrota y 0 en un empate.
- **Cambiar el precio** actualiza el gasto del partido en Finanzas: lo crea si el precio pasa de 0 a más, lo actualiza si cambia y lo borra si pasa a 0.
- **Cambiar la fecha o la cancha** vuelve a pedir el clima real a Open-Meteo, si la cancha tiene coordenadas y el modo no es simple.
- **Cambiar la cantidad de goles o asistencias:** si pasa a 0, el detalle se limpia; si cambia, se ofrece volver a cargarlo.
- **Camiseta al modificar:** si el ID no es válido vuelve a preguntar, igual que la cancha; 0 cancela.
- **Eliminar Partido:** 0 cancela y, si el partido no existe, el mensaje queda en pantalla.

### Corregido
- "Modificar Todo" ponía en 0 la diferencia de gol del partido.
- "Fecha y Hora" guardaba cualquier texto como fecha.
- Goles, asistencias y precio aceptaban valores negativos y guardaban -1 cuando se agotaban los intentos. Ahora los rangos son 0–99 para goles y asistencias y 0–1.000.000 para el precio, y -1 cancela sin guardar.
- Al modificar se informaba "modificado correctamente" aunque el guardado hubiera fallado.
- El control de duplicados de Finanzas no encontraba nunca el gasto del partido: buscaba con la categoría equivocada (6 en vez de 5, Canchas) y con un texto que no coincidía con el que se guarda. Esto pasaba al crear un partido y al cargar un gasto de cancha a mano.
- Varios pedidos de números (cancha, estado de cancha, arbitraje, goles en contra y los de crear partido) quedaban en un bucle infinito si se cerraba la entrada.
