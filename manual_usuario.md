# Manual de Usuario - MiFutbolC

## Introducción

Bienvenido a **MiFutbolC**, el sistema completo de gestión y análisis de datos para fútbol desarrollado en lenguaje C. Esta aplicación profesional le permite administrar todos los aspectos relacionados con el fútbol, desde la gestión de equipamiento hasta el análisis avanzado de rendimiento.

![Logo MiFutbolC](images/MiFutbolC.png)

### ¿Qué es MiFutbolC?

MiFutbolC es una herramienta integral diseñada para:
- Gestionar camisetas, canchas y partidos de fútbol
- Analizar estadísticas y rendimiento de manera profesional
- Realizar seguimiento de lesiones y salud de jugadores
- Exportar e importar datos en múltiples formatos
- Ofrecer un sistema gamificado de logros y recompensas

### Beneficios Clave

**Gestión Completa**: Todo lo relacionado con el fútbol en un solo lugar
**Análisis Profesional**: Estadísticas avanzadas y meta-análisis
**Seguimiento de Salud**: Gestión detallada de lesiones
**Exportación Flexible**: Múltiples formatos para diferentes necesidades
**Sistema Gamificado**: Logros y badges para motivar el uso continuo
**Personalización**: Configuración de usuario y preferencias

## Requisitos del Sistema

- **Sistema Operativo**: Windows, Linux o macOS
- **Compilador C**: GCC o MinGW
- **Herramientas Adicionales**:
  - CodeBlocks (recomendado)
  - Pandoc (para generar este manual en PDF)

## Instalación y Compilación

### Opción 1: Instalador Automático (Recomendado)

1. Navega a la carpeta `installer/` del proyecto
2. Ejecuta el archivo `MiFutbolC_Setup.exe`
3. Sigue las instrucciones del instalador
4. El programa se instalará automáticamente con todos los archivos necesarios

### Opción 2: Usando CodeBlocks

1. Descarga e instala CodeBlocks desde [codeblocks.org](https://www.codeblocks.org/)
2. Abre el archivo `MiFutbolC.cbp` con CodeBlocks
3. Selecciona "Build" > "Build" para compilar
4. El ejecutable se generará en `bin/Debug/MiFutbolC.exe`

### Opción 3: Compilación Manual

```bash
gcc -o MiFutbolC main.c db.c menu.c camiseta.c partido.c estadisticas.c analisis.c cancha.c logros.c lesion.c export.c export_all.c import.c utils.c sqlite3.c cJSON.c cJSON_Utils.c -I.
```

### Opción 4: Usando el Script Bash (Linux)

1. Asegúrate de tener GCC instalado.
2. Navega al directorio raíz del proyecto.
3. Haz el script ejecutable y ejecútalo:

```bash
chmod +x build.sh
./build.sh
```

Este script compila automáticamente todos los archivos fuente con advertencias habilitadas y símbolos de depuración, y ejecuta el programa si la compilación es exitosa.

## Primer Uso

Al ejecutar el programa por primera vez, se te pedirá que ingreses tu nombre de usuario. Este nombre se guardará para futuras sesiones.

![Pantalla de bienvenida](images/bienvenido.png)

## Menú Principal

El menú principal ofrece las siguientes opciones:

1. **Camisetas** - Gestionar camisetas de fútbol
2. **Canchas** - Gestionar canchas de fútbol
3. **Partidos** - Gestionar partidos
4. **Equipos** - Gestionar equipos de fútbol
5. **Estadísticas** - Ver estadísticas generales y avanzadas
6. **Logros** - Gestionar logros y badges
7. **Análisis** - Ver análisis de rendimiento
8. **Lesiones** - Gestionar lesiones de jugadores
9. **Financiamiento** - Gestionar finanzas del equipo (ingresos y gastos)
10. **Exportar** - Menú de exportación con opciones individuales por módulo
11. **Importar** - Importar datos desde archivos JSON
12. **Torneos** - Gestionar torneos de fútbol
13. **Ajustes** - Configurar temas de interfaz e idioma
0. **Salir** - Cerrar el programa

![Menú principal](images/menu.png)

## Gestión de Camisetas

### Crear una Camiseta

1. Selecciona "1" en el menú principal
2. Elige "1" para crear una nueva camiseta
3. Ingresa el nombre de la camiseta
4. La camiseta se guardará en la base de datos

### Listar Camisetas

1. Selecciona "1" en el menú principal
2. Elige "2" para listar todas las camisetas

### Editar una Camiseta

1. Selecciona "1" en el menú principal
2. Elige "3" para editar una camiseta
3. Ingresa el ID de la camiseta a editar
4. Modifica el nombre según sea necesario

### Eliminar una Camiseta

1. Selecciona "1" en el menú principal
2. Elige "4" para eliminar una camiseta
3. Ingresa el ID de la camiseta a eliminar
4. Confirma la eliminación

![Gestión de camisetas](images/menucamisetas.png)

## Gestión de Canchas

### Crear una Cancha

1. Selecciona "2" en el menú principal
2. Elige "1" para crear una nueva cancha
3. Ingresa el nombre de la cancha
4. Ingresa la ubicación de la cancha

### Listar Canchas

1. Selecciona "2" en el menú principal
2. Elige "2" para listar todas las canchas

### Editar una Cancha

1. Selecciona "2" en el menú principal
2. Elige "3" para editar una cancha
3. Ingresa el ID de la cancha a editar
4. Modifica el nombre y ubicación según sea necesario

### Eliminar una Cancha

1. Selecciona "2" en el menú principal
2. Elige "4" para eliminar una cancha
3. Ingresa el ID de la cancha a eliminar
4. Confirma la eliminación

![Gestión de canchas](images/menucanchas.png)

## Gestión de Partidos

### Crear un Partido

1. Selecciona "3" en el menú principal
2. Elige "1" para crear un nuevo partido
3. Selecciona la cancha
4. Ingresa la fecha y hora
5. Ingresa los goles, asistencias, rendimiento, cansancio, ánimo
6. Selecciona la camiseta utilizada

### Listar Partidos

1. Selecciona "3" en el menú principal
2. Elige "2" para listar todos los partidos

### Modificar un Partido

1. Selecciona "3" en el menú principal
2. Elige "3" para modificar un partido
3. Ingresa el ID del partido a modificar
4. Actualiza los datos según sea necesario

### Eliminar un Partido

1. Selecciona "3" en el menú principal
2. Elige "4" para eliminar un partido
3. Ingresa el ID del partido a eliminar
4. Confirma la eliminación

![Gestión de partidos](images/menupartidos.png)

## Gestión de Equipos

Selecciona "3" en el menú principal para acceder al menú de gestión de equipos. Este menú permite crear, gestionar y administrar equipos de fútbol con diferentes configuraciones.

### Crear un Equipo

1. Selecciona "3" en el menú principal
2. Elige "1" para crear un nuevo equipo
3. Selecciona el tipo de equipo (Fijo o Momentáneo)
4. Elige la modalidad de fútbol (Fútbol 5, 7, 8 o 11)
5. Ingresa el nombre del equipo
6. Agrega jugadores con sus posiciones (Arquero, Defensor, Mediocampista, Delantero)
7. Designa un capitán si es necesario
8. El equipo se guardará en la base de datos (si es fijo)

### Listar Equipos

1. Selecciona "3" en el menú principal
2. Elige "2" para listar todos los equipos
3. Se mostrarán todos los equipos con sus jugadores y formaciones

### Modificar un Equipo

1. Selecciona "3" en el menú principal
2. Elige "3" para modificar un equipo
3. Ingresa el ID del equipo a modificar
4. Actualiza la información del equipo y sus jugadores

### Eliminar un Equipo

1. Selecciona "3" en el menú principal
2. Elige "4" para eliminar un equipo
3. Ingresa el ID del equipo a eliminar
4. Confirma la eliminación

![Gestión de equipos](images/menuequipos.png)

## Gestión de Torneos

Selecciona "4" en el menú principal para acceder al menú de gestión de torneos. Este menú permite organizar y administrar competiciones futbolísticas completas.

### Crear un Torneo

1. Selecciona "4" en el menú principal
2. Elige "1" para crear un nuevo torneo
3. Ingresa el nombre del torneo
4. Selecciona si tiene equipo fijo
5. Elige el tipo de torneo (Ida y Vuelta, Solo Ida, Eliminación Directa, Grupos y Eliminación)
6. Selecciona el formato (Round Robin, Grupos con Final, etc.)
7. Especifica la cantidad de equipos participantes

### Listar Torneos

1. Selecciona "4" en el menú principal
2. Elige "2" para listar todos los torneos

### Modificar un Torneo

1. Selecciona "4" en el menú principal
2. Elige "3" para modificar un torneo
3. Ingresa el ID del torneo a modificar
4. Actualiza la configuración del torneo

### Eliminar un Torneo

1. Selecciona "4" en el menú principal
2. Elige "4" para eliminar un torneo
3. Ingresa el ID del torneo a eliminar
4. Confirma la eliminación

### Administrar un Torneo

1. Selecciona "4" en el menú principal
2. Elige "5" para administrar un torneo
3. Selecciona el torneo a administrar
4. Accede a funciones avanzadas:
   - Ver fixture
   - Ingresar resultados
   - Ver tabla de posiciones
   - Ver estado de equipos
   - Mostrar dashboard
   - Gestionar estadísticas de jugadores

### Asociar Equipos a un Torneo

1. Desde el menú de administración de torneo
2. Selecciona la opción para asociar equipos
3. Elige equipos existentes para incluir en el torneo

### Ver Fixture y Resultados

1. Desde el menú de administración
2. Selecciona "Ver Fixture" para ver los partidos programados
3. Selecciona "Ingresar Resultado" para registrar resultados de partidos

### Tabla de Posiciones

1. Desde el menú de administración
2. Selecciona "Ver Tabla de Posiciones"
3. Visualiza la clasificación actual del torneo

### Estadísticas de Torneo

1. Desde el menú de administración
2. Selecciona "Mostrar Estadísticas de Jugador"
3. Ve los mejores goleadores, asistidores y otras estadísticas

![Gestión de torneos](images/menutorneos.png)

## Estadísticas

Selecciona "4" en el menú principal para acceder al menú de estadísticas. Este menú ofrece una amplia variedad de análisis estadísticos, incluyendo estadísticas generales, análisis detallados de estados físicos y mentales, estadísticas históricas por año y mes, estadísticas avanzadas y meta-análisis, y récords y rankings.

### Estadísticas Generales

- Camiseta con más goles
- Camiseta con más asistencias
- Camiseta con más partidos
- Camiseta con más goles + asistencias
- Camiseta con mejor rendimiento general promedio
- Camiseta con mejor estado de ánimo promedio
- Camiseta con menos cansancio promedio
- Camiseta con más victorias
- Camiseta con más empates
- Camiseta con más derrotas
- Camiseta más sorteada

### Estadísticas por Año y Mes

- **Estadísticas por Año**: Análisis histórico agrupado por año, mostrando partidos jugados, goles, asistencias y promedios por camiseta.
- **Estadísticas por Mes**: Análisis histórico agrupado por mes, mostrando estadísticas detalladas por camiseta.

### Estadísticas Avanzadas y Meta-Análisis

El menú incluye funciones avanzadas de análisis profundo:

- **Consistencia del Rendimiento**: Análisis de variabilidad, desviación estándar y coeficiente de variación del rendimiento.
- **Partidos Atípicos**: Identificación de partidos con rendimiento excepcionalmente alto o bajo.
- **Dependencia del Contexto**: Análisis de cómo el rendimiento varía según clima, día de semana y resultado.
- **Impacto Real del Cansancio**: Correlación entre cansancio y rendimiento, incluyendo resultados por nivel de cansancio.
- **Impacto Real del Estado de Ánimo**: Correlación entre estado de ánimo y rendimiento, con análisis por niveles.
- **Eficiencia: Goles por Partido vs Rendimiento**: Relación entre producción de goles y rendimiento general.
- **Eficiencia: Asistencias vs Cansancio**: Cómo el cansancio afecta la capacidad de asistir.
- **Rendimiento por Esfuerzo**: Análisis de rendimiento obtenido por unidad de cansancio.
- **Partidos Exigentes Bien Rendidos**: Partidos difíciles con buen rendimiento.
- **Partidos Fáciles Mal Rendidos**: Partidos fáciles con bajo rendimiento.

### Récords y Rankings

Sistema completo de récords históricos:

- **Récords Individuales**: Máximo de goles y asistencias en un partido.
- **Combinaciones Óptimas**: Mejor y peor combinación cancha + camiseta.
- **Temporadas**: Mejor y peor temporada por rendimiento promedio.
- **Rendimiento Extremo**: Partidos con mejor y peor rendimiento general.
- **Combinaciones**: Partidos con mejor combinación de goles + asistencias.
- **Partidos Especiales**: Partidos sin goles, sin asistencias, rachas goleadoras y no goleadoras.
- **Rachas**: Mejor racha goleadora y peor racha (sin goles).

### Análisis de Estados Físicos y Mentales

El menú de estadísticas incluye funciones avanzadas para analizar el impacto de los estados físicos y mentales:

- **Rendimiento por Nivel de Cansancio**: Analiza el rendimiento promedio según niveles de cansancio (Bajo 1-3, Medio 4-7, Alto 8-10)
- **Goles con Cansancio Alto vs Bajo**: Compara los goles marcados con cansancio alto versus bajo (usando el promedio como referencia)
- **Partidos con Cansancio Alto**: Muestra el número total de partidos jugados con nivel de cansancio mayor a 7
- **Caída de Rendimiento por Cansancio Acumulado**: Compara el rendimiento en los partidos recientes con alto cansancio vs partidos antiguos con alto cansancio
- **Rendimiento por Estado de Animo**: Analiza el rendimiento promedio según niveles de estado de ánimo (Bajo 1-3, Medio 4-7, Alto 8-10)
- **Goles por Estado de Animo**: Muestra los goles marcados según el estado de ánimo del jugador
- **Asistencias por Estado de Animo**: Analiza las asistencias realizadas según el estado de ánimo
- **Estado de Animo Ideal para Jugar**: Identifica el nivel de estado de ánimo que produce el mejor rendimiento promedio

### Estadísticas por Clima y Día de la Semana

- **Rendimiento Promedio por Clima**: Análisis del rendimiento según condiciones climáticas
- **Goles por Clima**: Total de goles marcados en diferentes climas
- **Asistencias por Clima**: Asistencias realizadas según clima
- **Clima Mejor Rendimiento**: Identificación del clima donde se rinde mejor
- **Clima Peor Rendimiento**: Identificación del clima donde se rinde peor
- **Mejor Día de la Semana**: Día con mejor rendimiento promedio
- **Peor Día de la Semana**: Día con peor rendimiento promedio
- **Goles Promedio por Día**: Goles marcados por día de la semana
- **Asistencias Promedio por Día**: Asistencias por día de la semana
- **Rendimiento Promedio por Día**: Rendimiento general por día de la semana

### Estadísticas de Lesiones

El módulo de estadísticas de lesiones proporciona un análisis completo de las lesiones registradas:

- **Total de Lesiones**: Número total de incidentes médicos registrados
- **Lesiones por Tipo**: Clasificación de lesiones por categorías diagnósticas
- **Lesiones por Camiseta**: Distribución de lesiones por jugador/camiseta
- **Lesiones por Mes**: Análisis temporal mensual de incidentes médicos
- **Mes con Más Lesiones**: Identificación del período de mayor riesgo lesional
- **Tiempo Promedio entre Lesiones**: Cálculo de intervalos promedio entre lesiones consecutivas
- **Rendimiento Promedio Antes/Después de Lesiones**: Comparación de métricas de producción (goles + asistencias) antes y después de incidentes médicos

![Estadísticas](images/menuestadisticas.png)

## Logros

Selecciona "5" en el menú principal para acceder al sistema de logros y badges. Los logros están organizados por categorías y niveles de dificultad.

### Ver Todos los Logros

1. Selecciona "5" en el menú principal
2. Elige "1" para ver todos los logros

### Ver Logros Completados

1. Selecciona "5" en el menú principal
2. Elige "2" para ver logros completados

### Ver Logros en Progreso

1. Selecciona "5" en el menú principal
2. Elige "3" para ver logros en progreso

![Logros](images/menulogros.png)

## Análisis de Rendimiento

Selecciona "6" en el menú principal para ver el análisis de rendimiento, que incluye:

- Comparación de los últimos 5 partidos con promedios generales
- Cálculo de rachas de victorias y derrotas
- Análisis motivacional basado en el rendimiento

## Gestión de Lesiones

### Registrar una Lesión

1. Selecciona "7" en el menú principal
2. Elige "1" para registrar una nueva lesión
3. Ingresa el nombre del jugador
4. Selecciona el tipo de lesión
5. Ingresa la fecha y duración

### Listar Lesiones

1. Selecciona "7" en el menú principal
2. Elige "2" para listar todas las lesiones

### Editar una Lesión

1. Selecciona "7" en el menú principal
2. Elige "3" para editar una lesión
3. Ingresa el ID de la lesión a editar
4. Modifica los datos según sea necesario

### Eliminar una Lesión

1. Selecciona "7" en el menú principal
2. Elige "4" para eliminar una lesión
3. Ingresa el ID de la lesión a eliminar
4. Confirma la eliminación

![Gestión de lesiones](images/menulesiones.png)

## Gestión Financiera

Selecciona "9" en el menú principal para acceder al módulo de gestión financiera del equipo. Este módulo permite registrar y controlar todos los ingresos y gastos relacionados con el fútbol.

### Agregar Transacción Financiera

1. Selecciona "9" en el menú principal
2. Elige "1" para agregar una nueva transacción
3. Selecciona el tipo (Ingreso o Gasto)
4. Elige la categoría apropiada:
   - Transporte
   - Equipamiento
   - Cuotas
   - Torneos
   - Arbitraje
   - Canchas
   - Medicina
   - Otros
5. Ingresa la descripción y monto
6. Especifica el item específico si aplica

### Listar Transacciones

1. Selecciona "9" en el menú principal
2. Elige "2" para ver todas las transacciones financieras

### Modificar Transacción

1. Selecciona "9" en el menú principal
2. Elige "3" para modificar una transacción existente
3. Ingresa el ID de la transacción a modificar

### Eliminar Transacción

1. Selecciona "9" en el menú principal
2. Elige "4" para eliminar una transacción
3. Confirma la eliminación

### Ver Resumen Financiero

1. Selecciona "9" en el menú principal
2. Elige "5" para ver un resumen completo de ingresos y gastos

### Ver Balance de Gastos

1. Selecciona "9" en el menú principal
2. Elige "6" para analizar el balance por categorías

### Exportar Datos Financieros

1. Selecciona "9" en el menú principal
2. Elige "7" para exportar las transacciones financieras

![Gestión financiera](images/menufinanciamiento.png)

## Simulación de Partidos

El sistema incluye una función avanzada de simulación de partidos que permite crear equipos momentáneos y simular partidos completos con visualización en tiempo real.

### Crear Equipos para Simulación

1. Ve al menú de Equipos (opción 4)
2. Selecciona "Crear Equipo Momentáneo"
3. Elige "Dos equipos (Local y Visitante)"
4. Crea ambos equipos con sus jugadores y posiciones

### Gestionar Equipos

Para cada equipo puedes:
- Modificar jugadores existentes
- Agregar nuevos jugadores
- Eliminar jugadores
- Cambiar el capitán

### Iniciar Simulación

1. Desde el menú de gestión de equipos momentáneos
2. Selecciona "3. Simular partido"
3. La simulación comenzará automáticamente

### Cómo Funciona la Simulación

- **Duración**: 60 minutos simulados
- **Eventos**: Goles, asistencias, oportunidades y faltas generadas aleatoriamente
- **Visualización**: Cancha de fútbol animada en ASCII
- **Estadísticas**: Registro detallado de todos los eventos por jugador
- **Resultado**: Marcador final y resumen completo

### Características de la Simulación

- Cancha animada con balón en movimiento
- Eventos en tiempo real (cada segundo = un minuto de partido)
- Estadísticas detalladas de goles y asistencias
- Determinación automática de resultado
- No se guardan datos (equipos momentáneos)

## Configuración (Ajustes)

Selecciona "13" en el menú principal para acceder al menú de configuración del sistema.

### Cambiar Tema de Interfaz

1. Selecciona "13" en el menú principal
2. Elige "1" para cambiar el tema
3. Selecciona uno de los temas disponibles:
   - Claro
   - Oscuro
   - Azul
   - Verde
   - Rojo
   - Púrpura
   - Clásico
   - Alto Contraste

### Cambiar Idioma

1. Selecciona "13" en el menú principal
2. Elige "2" para cambiar el idioma
3. Selecciona entre Español e Inglés

### Ver Configuración Actual

1. Selecciona "13" en el menú principal
2. Elige "3" para ver la configuración actual

### Restablecer Valores por Defecto

1. Selecciona "13" en el menú principal
2. Elige "4" para restablecer configuración por defecto

![Menú de ajustes](images/menuajustes.png)

## Exportar Datos

Selecciona "8" en el menú principal para acceder al menú de exportación. Este menú ofrece opciones para exportar datos de diferentes módulos por separado:

1. **Camisetas** - Exportar datos de camisetas
2. **Partidos** - Exportar datos de partidos (incluyendo submenú para partidos específicos)
3. **Lesiones** - Exportar datos de lesiones
4. **Estadísticas** - Exportar estadísticas generales
5. **Análisis** - Exportar análisis de rendimiento
6. **Todo** - Exportar todos los datos
7. **Analisis Avanzado** - Exportación mejorada con análisis integrado

### Submenú de Exportar Partidos

El menú de exportar partidos incluye opciones adicionales:
- **Todos los Partidos** - Exportar todos los partidos
- **Partido con Mas Goles** - Exportar el partido con más goles
- **Partido con Mas Asistencias** - Exportar el partido con más asistencias
- **Partido Menos Goles Reciente** - Exportar el partido más reciente con menos goles
- **Partido Menos Asistencias Reciente** - Exportar el partido más reciente con menos asistencias

### Submenú de Exportar Estadísticas Generales

El menú de exportar estadísticas generales incluye:
- **Estadisticas Generales** - Exportar estadísticas generales completas
- **Estadisticas Por Mes** - Exportar estadísticas por mes
- **Estadisticas Por Anio** - Exportar estadísticas por año
- **Records & Rankings** - Exportar récords y rankings

### Exportación Mejorada

La opción "Analisis Avanzado" proporciona exportación mejorada con:
- **Camisetas con Analisis Avanzado** - Exportación de camisetas con análisis integrado que incluye estadísticas avanzadas como eficiencia de goles/asistencias, porcentaje de victorias, análisis de lesiones y métricas de rendimiento
- **Lesiones con Analisis de Impacto** - Exportación de lesiones con análisis de impacto que incluye evaluación de gravedad de lesiones, comparación de rendimiento antes/después de lesiones e identificación de patrones de lesiones
- **Todo con Analisis Avanzado** - Exportación completa con análisis avanzado que combina todas las funcionalidades mejoradas

Cada opción permite exportar en múltiples formatos (CSV, TXT, JSON, HTML). Los archivos se guardarán en el Escritorio (Windows) o directorio home (Unix/Linux) con nombres descriptivos como `camisetas.csv`, `partidos.html`, etc. El usuario puede elegir el formato para cada módulo.

![Exportar datos](images/menuexportar.png)

## Importar Datos

Selecciona "9" en el menú principal para importar datos Los archivos deben estar ubicados en el directorio `Documents/MiFutbolC/Importaciones` (o `%USERPROFILE%\Documents\MiFutbolC\Importaciones` en Windows, `./importaciones` en el directorio del ejecutable en Unix/Linux) con nombres específicos, generados por la función de exportación.

![Importar datos](images/menuimportar.png)

## Gestión de Usuario

Selecciona "10" en el menú principal para cambiar el nombre de usuario o ver información del usuario actual.

![Gestión de usuario](images/menuusuarios.png)

## Consejos de Uso

- Siempre confirma las operaciones de eliminación
- Utiliza la función de exportación regularmente para hacer copias de seguridad
- Revisa las estadísticas y análisis para mejorar el rendimiento
- Completa logros para motivarte a usar la aplicación

## Solución de Problemas

### El programa no se ejecuta

- Verifica que el archivo ejecutable existe en `bin/Debug/MiFutbolC.exe`
- Asegúrate de tener permisos para ejecutar archivos en el directorio

### Error al conectar con la base de datos

- Verifica que el directorio `data/` (o `%APPDATA%\MiFutbolC\data\` en Windows, `data/` en el directorio del ejecutable en Unix/Linux) existe y tienes permisos de escritura
- El programa creará automáticamente la base de datos `mifutbol.db` si no existe

### Datos no se guardan

- Verifica que no hay errores en la consola
- Revisa que la base de datos no esté corrupta

## Conclusión

MiFutbolC es una herramienta completa para el seguimiento y análisis de datos relacionados con el fútbol. Con su interfaz intuitiva y funcionalidades avanzadas, te permite gestionar todos los aspectos de tu experiencia futbolística.

¡Disfruta usando MiFutbolC!

---

*Manual generado con Pandoc*
*Última actualización: (09/01/2026)*
