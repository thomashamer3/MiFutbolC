# Manual de Usuario - MiFutbolC

## Introducción

Bienvenido a **MiFutbolC**, el sistema completo de gestión y análisis de datos para fútbol desarrollado en lenguaje C. Esta aplicación profesional le permite administrar todos los aspectos relacionados con el fútbol, desde la gestión de equipamiento hasta el análisis avanzado de rendimiento.

![Logo MiFutbolC](images/MiFutbolC.png)

### ¿Qué es MiFutbolC?

MiFutbolC es una herramienta integral diseñada para:
- Gestionar camisetas, canchas, equipos y partidos de fútbol
- Analizar estadísticas y rendimiento de manera profesional
- Realizar seguimiento de lesiones y salud de jugadores
- Gestionar finanzas del equipo (ingresos y gastos)
- Organizar torneos y temporadas completas
- Exportar e importar datos en múltiples formatos
- Ofrecer un sistema gamificado de logros y recompensas
- Registrar bienestar integral (hábitos, salud y planificación)
- Gestionar recordatorios y agenda personal
- Administrar colecciones e inventario de ítems
- Recibir recomendaciones inteligentes del Entrenador IA

### Beneficios Clave

**Gestión Completa**: Todo lo relacionado con el fútbol en un solo lugar  
**Análisis Profesional**: Estadísticas avanzadas y meta-análisis  
**Seguimiento de Salud**: Gestión detallada de lesiones  
**Control Financiero**: Registro de ingresos y gastos por categorías  
**Gestión de Temporadas**: Seguimiento de ciclos deportivos completos  
**Exportación Flexible**: Múltiples formatos (CSV, JSON, HTML, TXT)  
**Informe PDF Mejorado**: Portada, secciones y más datos  
**Sistema Gamificado**: Logros y badges para motivar el uso continuo  
**Bienestar Integral**: Hábitos, salud y reportes personales  
**Entrenador IA**: Recomendaciones inteligentes basadas en datos  
**Personalización**: Configuración de temas, idioma y preferencias  
- **Rutas de base de datos más robustas**: mejora interna en la construcción de rutas usando `snprintf`.
- **Logs más confiables**: ajustes en el formateo de mensajes para evitar inconsistencias en salidas de diagnóstico.
- **Gestión de imágenes optimizada**: mejoras en selección, resolución y actualización de rutas de imágenes de camisetas.
- **Mejor feedback en herramientas opcionales**: mensajes de éxito/advertencia más claros al configurar utilidades de imagen.

## Requisitos del Sistema

- **Sistema Operativo**: Windows, Linux o macOS
- **Compilador C**: GCC o MinGW
- **Herramientas Adicionales**:
  - CodeBlocks (recomendado para desarrollo)
  - Pandoc (opcional, para generar este manual en PDF)

## Instalación y Compilación

### Opción 1: Instalador Automático (Windows - Recomendado)

1. Navega a la carpeta `installer/` del proyecto
2. Ejecuta el archivo `MiFutbolC_Setup.exe`
3. Sigue las instrucciones del instalador
4. El programa se instalará automáticamente con todos los archivos necesarios
5. Busca "MiFutbolC" en el menú de inicio de Windows

### Opción 2: Usando CodeBlocks

1. Descarga e instala CodeBlocks desde [codeblocks.org](https://www.codeblocks.org/)
2. Abre el archivo `MiFutbolC.cbp` con CodeBlocks
3. Selecciona "Build" > "Build" para compilar (o presiona `Ctrl+F9`)
4. El ejecutable se generará en `bin/Debug/MiFutbolC.exe`
5. Ejecuta con `F9` o "Build" > "Build and Run"

### Opción 3: Compilación Manual

#### Linux / macOS (script)

```bash
# Compilar todos los archivos fuente
gcc -o MiFutbolC \
    main.c db.c menu.c camiseta.c partido.c equipo.c torneo.c \
    estadisticas.c estadisticas_generales.c estadisticas_anio.c \
    estadisticas_mes.c estadisticas_meta.c estadisticas_lesiones.c \
    analisis.c cancha.c logros.c lesion.c temporada.c \
    financiamiento.c settings.c entrenador_ia.c \
    records_rankings.c export.c export_all.c export_all_mejorado.c \
    export_camisetas.c export_camisetas_mejorado.c \
    export_lesiones.c export_lesiones_mejorado.c \
    export_partidos.c export_estadisticas.c \
    export_estadisticas_generales.c export_records_rankings.c \
    import.c utils.c sqlite3.c cJSON.c \
    -I. -lm -lpthread -ldl

# Ejecutar el programa
./MiFutbolC
```

#### Windows (con MinGW)

```bash
gcc -o MiFutbolC.exe main.c db.c menu.c camiseta.c partido.c equipo.c torneo.c estadisticas.c estadisticas_generales.c estadisticas_anio.c estadisticas_mes.c estadisticas_meta.c estadisticas_lesiones.c analisis.c cancha.c logros.c lesion.c temporada.c financiamiento.c settings.c entrenador_ia.c records_rankings.c export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_estadisticas.c export_estadisticas_generales.c export_records_rankings.c import.c utils.c sqlite3.c cJSON.c -I.
```

### Opción 4: Usando scripts del proyecto

#### Linux / macOS

```bash
chmod +x Instalador-Linux.sh
./Instalador-Linux.sh

# Debug
./Instalador-Linux.sh --debug

# Compilar y ejecutar
./Instalador-Linux.sh run
```

#### Windows (script)

```bash
mingw32-make

# Debug
mingw32-make BUILD_TYPE=Debug
```

#### Windows (instalación por consola)

```powershell
# Instalación estándar (compila + instala)
powershell -ExecutionPolicy Bypass -File .\install.ps1

# Debug
powershell -ExecutionPolicy Bypass -File .\install.ps1 -BuildType Debug

# Instalar usando un .exe ya compilado
powershell -ExecutionPolicy Bypass -File .\install.ps1 -SkipBuild
```

`install.ps1` instala en `%LOCALAPPDATA%\Programs\MiFutbolC`, crea un launcher de consola (`MiFutbolC.cmd`) y puede agregar la ruta de instalación al `PATH` del usuario.

En Linux/macOS, `Instalador-Linux.sh` también puede validar dependencias e instalar un launcher en `PATH`.

## Primer Uso

Al ejecutar el programa por primera vez:

1. Se abrirá el flujo de **inicio de sesión multiusuario local**
2. Podrás crear tu primer usuario local (o elegir uno existente)
3. Cada perfil usará su propia base de datos SQLite
   - **Windows**: `%LOCALAPPDATA%\MiFutbolC\data\mifutbol_<usuario>.db`
   - **Linux/macOS**: `./data/mifutbol_<usuario>.db`

   Además, se utiliza un registro de usuarios locales:
   - **Windows**: `%LOCALAPPDATA%\MiFutbolC\data\users.db`
   - **Linux/macOS**: `./data/users.db`

   También se crea automáticamente el archivo de log de actividad:
   - **Windows**: `%LOCALAPPDATA%\MiFutbolC\data\mifutbol_<usuario>.log`
   - **Linux/macOS**: `./data/mifutbol_<usuario>.log`

4. Podrás definir contraseña opcional para tu perfil
5. Se mostrarán los directorios de exportación e importación

![Pantalla de bienvenida](images/bienvenido.png)

## Menú Principal

El menú principal ofrece las siguientes opciones:

1. **Dashboard** - Ver resumen general del sistema
2. **Calendario** - Ver eventos, partidos y recordatorios
3. **Camisetas** - Gestionar camisetas de fútbol
4. **Canchas** - Gestionar canchas de fútbol
5. **Equipos** - Gestionar equipos (fijos y momentáneos)
6. **Partidos** - Gestionar partidos
7. **Lesiones** - Gestionar lesiones de jugadores
8. **Estadísticas** - Ver estadísticas por categorías y rendimiento
9. **Logros** - Gestionar logros y badges
10. **Financiamiento** - Gestionar finanzas del equipo
11. **Torneos** - Gestionar torneos de fútbol
12. **Temporada** - Gestionar temporadas y ciclos deportivos
13. **Análisis** - Ver análisis de rendimiento, comparador y Entrenador IA
14. **Bienestar** - Planificación, hábitos, salud y reportes personales
15. **Carrera Futbolística** - Seguimiento de progreso y objetivos de carrera
16. **Recordatorios** - Gestionar recordatorios, agenda y exportación/importación del módulo
17. **Colecciones** - Administrar ítems, colecciones y backups JSON
18. **Ajustes** - Configurar temas, idioma, accesibilidad y herramientas
19. **Música** - Reproductor integrado con soporte MP3/WAV/FLAC/OGG, ecualizador 3 bandas, playlists, filtro de búsqueda, sleep timer, seek exacto, información técnica de pista, renombrar pista, exportar catálogo y paso de volumen configurable
0. **Salir** - Cerrar el programa

![Menú principal](images/menu.png)

### ¿Qué puedes hacer en cada menú?

- **1. Dashboard**: ver resumen general de actividad, logros, balance y alertas rápidas.
- **2. Calendario**: navegar por meses, revisar eventos de un día y volver al día actual.
- **3. Camisetas**: crear, listar, editar, eliminar, sortear, gestionar imágenes e información de camisetas.
- **4. Canchas**: crear, listar, editar, eliminar, asociar imagen y activar/desactivar canchas.
- **5. Equipos**: crear, listar, editar, eliminar, cargar/ver imagen y activar/desactivar equipos.
- **6. Partidos**: crear, listar, editar, eliminar, simular con equipos guardados y usar análisis táctico.
- **7. Lesiones**: crear, listar, editar, eliminar, consultar estadísticas y actualizar estados.
- **8. Estadísticas**: revisar generales, partidos, goles, asistencias y rendimiento.
- **9. Logros**: ver logros totales, completados, en progreso y no completados.
- **10. Financiamiento**: registrar transacciones, ver resúmenes, balance, exportar y gestionar presupuestos.
- **11. Torneos**: crear, listar, modificar y eliminar torneos.
- **12. Temporada**: crear, listar, modificar, eliminar, administrar y comparar temporadas.
- **13. Análisis**: usar análisis básico, comparador avanzado, diagramas tácticos, Entrenador IA y química.
- **14. Bienestar**: registrar planificación, hábitos, entrenamiento, alimentación, mental y salud.
- **15. Carrera Futbolística**: gestionar trayectoria, historial y resumen de carrera.
- **16. Recordatorios**: listar, agregar, editar, eliminar, filtrar, exportar/importar y abrir agenda.
- **17. Colecciones**: administrar inventario, colecciones, sincronización y backups JSON.
- **18. Ajustes**: personalizar tema, idioma, accesibilidad, usuario, modo, exportación/importación y actualización.
- **19. Música**: controlar reproducción, EQ, playlists, búsqueda, temporizador y gestión de archivos de audio.

## Dashboard

Selecciona "1" en el menú principal para acceder al Dashboard.

### Funcionalidades del Dashboard

El Dashboard ofrece una vista consolidada del sistema:

- **Resumen de partidos**: total, victorias, empates y derrotas
- **Últimos partidos**: los partidos más recientes con resultado y rendimiento
- **Estadísticas rápidas**: goles y asistencias totales, rendimiento promedio
- **Estado de logros**: logros recientes y progreso activo
- **Alertas y recordatorios próximos**: eventos cercanos destacados

![Dashboard](images/dashboard.png)

## Calendario

Selecciona "2" en el menú principal para acceder al Calendario.

### Funcionalidades del Calendario

El Calendario permite visualizar y gestionar la agenda deportiva:

- **Vista mensual**: eventos del mes actual con navegación entre meses
- **Partidos agendados**: fechas de partidos registrados
- **Recordatorios**: recordatorios con fecha mostrados en el calendario
- **Torneos activos**: fechas relevantes de torneos en curso

![Calendario](images/calendario.png)

## Gestión de Camisetas

### Qué puedes hacer en Camisetas

- **1. Crear**: registrar una camiseta nueva.
- **2. Listar**: ver camisetas activas/inactivas y sus datos.
- **3. Modificar**: actualizar nombre u otra información.
- **4. Eliminar**: eliminar, reasignar historial o retirar camiseta.
- **5. Sortear**: elegir aleatoriamente una camiseta para jugar.
- **6. Cargar Imagen**: asociar una imagen al registro.
- **7. Ver Camiseta**: abrir la imagen asignada.
- **8. Ajustes Imagen**: configurar/previsualizar visor de imágenes.
- **9. Ver Información**: consultar detalles ampliados.
- **10. Cargar Información**: completar o actualizar metadatos.
- **11. Reactivar/Desactivar**: cambiar estado operativo.

### Crear una Camiseta

1. Selecciona "3" en el menú principal
2. Elige "1" para crear una nueva camiseta
3. Ingresa el nombre de la camiseta
4. La camiseta se guardará en la base de datos con ID único

### Listar Camisetas

1. Selecciona "3" en el menú principal
2. Elige "2" para listar todas las camisetas
3. Se mostrarán todas las camisetas con sus estadísticas de uso

### Editar una Camiseta

1. Selecciona "3" en el menú principal
2. Elige "3" para editar una camiseta
3. Ingresa el ID de la camiseta a editar
4. Modifica el nombre según sea necesario

### Eliminar una Camiseta

1. Selecciona "3" en el menú principal
2. Elige "4" para eliminar una camiseta
3. Ingresa el ID de la camiseta a eliminar
4. Si la camiseta tiene partidos asociados, el sistema ofrece:
   - Reasignar partidos a otra camiseta activa y eliminar la original
   - Eliminar la camiseta junto con todos sus partidos asociados
   - Retirar camiseta (marcar inactiva y conservar historial)
5. Confirma la operación escribiendo la frase de validación solicitada

### Ajustes de Imagen de Camiseta

1. Selecciona "3" en el menú principal
2. Elige "8" (**Ajustes Imagen**)
3. Opciones disponibles:
   - Configurar visor preferido
   - Probar visor actual
   - Previsualizar imagen en consola

### Reactivar Camiseta

1. Selecciona "3" en el menú principal
2. Elige "11" (**Reactivar**)
3. Ingresa el ID de la camiseta inactiva
4. Confirma para volver a marcarla como activa

### Sortear Camiseta

1. Selecciona "3" en el menú principal
2. Elige "5" para sortear una camiseta
3. El sistema seleccionará una camiseta disponible al azar
4. Si todas ya fueron sorteadas, reinicia automáticamente el ciclo

### Cargar Imagen de Camiseta

Cuando uses la opción de cargar imagen para una camiseta:

1. El sistema copiará la imagen al directorio `Imagenes/`
2. Si detecta un optimizador disponible (por ejemplo ImageMagick), aplicará optimización automática
3. La imagen se guarda en formato optimizado para reducir tamaño y mantener buena calidad
4. Si no hay optimizador, se guarda una copia sin optimización

![Gestión de camisetas](images/menucamisetas.png)

## Gestión de Canchas

### Qué puedes hacer en Canchas

- **1. Crear**: registrar una cancha.
- **2. Listar**: ver canchas guardadas y estado.
- **3. Modificar**: editar datos de una cancha.
- **4. Eliminar**: eliminar o retirar cancha con reglas de seguridad.
- **5. Cargar Imagen**: asociar imagen de referencia.
- **6. Ver Imagen**: abrir la imagen de la cancha.
- **7. Ver Información**: consultar ficha de la cancha.
- **8. Cargar Información**: ampliar o actualizar datos.
- **9. Reactivar/Desactivar Cancha**: cambiar estado sin perder historial.

### Crear una Cancha

1. Selecciona "4" en el menú principal
2. Elige "1" para crear una nueva cancha
3. Ingresa el nombre de la cancha

### Listar Canchas

1. Selecciona "4" en el menú principal
2. Elige "2" para listar todas las canchas
3. Se mostrarán con estadísticas de uso

### Editar una Cancha

1. Selecciona "4" en el menú principal
2. Elige "3" para editar una cancha
3. Ingresa el ID de la cancha a editar
4. Modifica el nombre según sea necesario

### Eliminar una Cancha

1. Selecciona "4" en el menú principal
2. Elige "4" para eliminar una cancha
3. Ingresa el ID de la cancha a eliminar
4. Si la cancha tiene partidos asociados, podrás:
   - Reasignar partidos a otra cancha activa y eliminar la original
   - Eliminar la cancha junto con todos sus partidos asociados
   - Retirar cancha (marcar inactiva y conservar historial)
5. Confirma la operación escribiendo la frase de validación solicitada

### Reactivar una Cancha

1. Selecciona "4" en el menú principal
2. Elige "9" (**Reactivar Cancha**)
3. Ingresa el ID de la cancha inactiva
4. Confirma para volver a marcarla como activa

![Gestión de canchas](images/menucanchas.png)

## Gestión de Equipos

Selecciona "5" en el menú principal para acceder al menú de gestión de equipos. Este módulo permite crear y administrar equipos de fútbol con diferentes configuraciones.

### Qué puedes hacer en Equipos

- **1. Crear**: crear equipos fijos o momentáneos.
- **2. Listar**: consultar planteles y formaciones.
- **3. Modificar**: editar datos del equipo y jugadores.
- **4. Eliminar**: borrar equipos.
- **5. Cargar Imagen**: asociar imagen al equipo.
- **6. Ver Imagen**: abrir la imagen del equipo.
- **7. Reactivar/Desactivar Equipo**: alternar estado activo.

### Crear un Equipo

1. Selecciona "5" en el menú principal
2. Elige "1" para crear un nuevo equipo
3. Selecciona el tipo de equipo:
   - **Fijo**: Se guarda permanentemente en la base de datos
   - **Momentáneo**: Solo para uso temporal/simulación
4. Elige la modalidad de fútbol:
   - Fútbol 5
   - Fútbol 7
   - Fútbol 8
   - Fútbol 11
5. Ingresa el nombre del equipo
6. Agrega jugadores con sus datos:
   - Nombre del jugador
   - Número de camiseta
   - Posición (Arquero, Defensor, Mediocampista, Delantero)
7. Designa un capitán si es necesario
8. Para equipos fijos, se guardará en la base de datos

### Listar Equipos

1. Selecciona "5" en el menú principal
2. Elige "2" para listar todos los equipos
3. Se mostrarán todos los equipos con sus jugadores y formaciones

### Modificar un Equipo

1. Selecciona "5" en el menú principal
2. Elige "3" para modificar un equipo
3. Ingresa el ID del equipo a modificar
4. Actualiza la información del equipo y sus jugadores

### Eliminar un Equipo

1. Selecciona "5" en el menú principal
2. Elige "4" para eliminar un equipo
3. Ingresa el ID del equipo a eliminar
4. Confirma la eliminación

![Gestión de equipos](images/menuequipos.png)

## Gestión de Partidos

### Qué puedes hacer en Partidos

- **1. Crear**: registrar partidos con métricas completas.
- **2. Listar**: consultar historial de partidos.
- **3. Modificar**: editar partido existente.
- **4. Eliminar**: borrar partido.
- **5. Simular con Equipos Guardados**: ejecutar simulación de encuentro.
- **6. Análisis Táctico**: crear/consultar diagramas tácticos.
- **7. Favoritos**: marcar partidos destacados para consulta rápida.
- **8. Etiquetas (Tags)**: clasificar partidos con etiquetas.

### Crear un Partido

1. Selecciona "6" en el menú principal
2. Elige "1" para crear un nuevo partido
3. Selecciona la cancha donde se jugó
4. Ingresa la fecha y hora del partido
5. Ingresa las estadísticas:
   - Goles marcados
   - Asistencias realizadas
   - Rendimiento general (1-10)
   - Nivel de cansancio (1-10)
   - Estado de ánimo (1-10)
   - Resultado (Victoria, Empate, Derrota)
   - Clima (Soleado, Nublado, Lluvioso, etc.)
   - Comentario personal (opcional)
6. Selecciona la camiseta utilizada
7. El partido se guardará con todos los datos

### Listar Partidos

1. Selecciona "6" en el menú principal
2. Elige "2" para listar todos los partidos
3. Se mostrarán con todas las estadísticas

### Modificar un Partido

1. Selecciona "6" en el menú principal
2. Elige "3" para modificar un partido
3. Ingresa el ID del partido a modificar
4. Actualiza los datos según sea necesario

### Eliminar un Partido

1. Selecciona "6" en el menú principal
2. Elige "4" para eliminar un partido
3. Ingresa el ID del partido a eliminar
4. Confirma la eliminación

### Simular con Equipos Guardados

1. Selecciona "6" en el menú principal
2. Elige "5" para simular un partido con equipos guardados
3. Sigue el flujo para elegir los equipos y ejecutar la simulación

### Análisis Táctico

1. Selecciona "6" en el menú principal
2. Elige "6" para abrir análisis táctico
3. Podrás crear y visualizar diagramas tácticos

![Gestión de partidos](images/menupartidos.png)

## Gestión de Lesiones

### Qué puedes hacer en Lesiones

- **1. Crear**: registrar una lesión nueva.
- **2. Listar**: consultar lesiones registradas.
- **3. Modificar**: editar lesión existente.
- **4. Eliminar**: eliminar lesión.
- **5. Estadísticas**: ver analítica de lesiones.
- **6. Diferencias entre Lesiones**: comparar tipos/incidencias.
- **7. Actualizar Estados**: actualizar evolución de recuperación.

### Registrar una Lesión

1. Selecciona "7" en el menú principal
2. Elige "1" para registrar una nueva lesión
3. Ingresa el nombre del jugador
4. Selecciona el tipo de lesión (Muscular, Articular, Ósea, etc.)
5. Ingresa una descripción detallada
6. Especifica la fecha de la lesión
7. Indica la duración estimada de recuperación
8. Selecciona la camiseta asociada (opcional)
9. Asocia con un partido si aplica

### Listar Lesiones

1. Selecciona "7" en el menú principal
2. Elige "2" para listar todas las lesiones
3. Se mostrarán con detalles completos y estado

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

### Estadísticas y Herramientas de Lesiones

1. Selecciona "7" en el menú principal
2. Usa opciones adicionales del menú de lesiones:
   - "5" Estadísticas
   - "6" Diferencias entre lesiones
   - "7" Actualizar estados

![Gestión de lesiones](images/menulesiones.png)

## Estadísticas

Selecciona "8" en el menú principal para acceder al menú de estadísticas. Este menú ofrece una amplia variedad de análisis estadísticos.

### Qué puedes hacer en Estadísticas

- **1. Generales**: ver métricas globales del rendimiento.
- **2. Partidos**: analizar indicadores centrados en partidos.
- **3. Goles**: evaluar producción goleadora.
- **4. Asistencias**: medir contribución en asistencias.
- **5. Rendimiento**: revisar tendencias y promedios de rendimiento.

### Estadísticas Generales

Análisis completo del rendimiento de camisetas:

- Camiseta con más goles
- Camiseta con más asistencias
- Camiseta con más partidos jugados
- Camiseta con más goles + asistencias
- Camiseta con mejor rendimiento general promedio
- Camiseta con mejor estado de ánimo promedio
- Camiseta con menos cansancio promedio
- Camiseta con más victorias
- Camiseta con más empates
- Camiseta con más derrotas
- Camiseta más sorteada

### Estadísticas por Año y Mes

- **Estadísticas por Año**: Análisis histórico agrupado por año, mostrando partidos jugados, goles, asistencias y promedios por camiseta
- **Estadísticas por Mes**: Análisis histórico agrupado por mes, mostrando estadísticas detalladas por camiseta

### Estadísticas Avanzadas y Meta-Análisis

Análisis profundo del rendimiento:

- **Consistencia del Rendimiento**: Análisis de variabilidad, desviación estándar y coeficiente de variación
- **Partidos Atípicos (Outliers)**: Identificación de partidos con rendimiento excepcionalmente alto o bajo
- **Dependencia del Contexto**: Análisis de cómo el rendimiento varía según clima, día de semana y resultado
- **Impacto Real del Cansancio**: Correlación entre cansancio y rendimiento, con resultados por nivel de cansancio
- **Impacto Real del Estado de Ánimo**: Correlación entre estado de ánimo y rendimiento, con análisis por niveles
- **Eficiencia: Goles por Partido vs Rendimiento**: Relación entre producción de goles y rendimiento general
- **Eficiencia: Asistencias vs Cansancio**: Cómo el cansancio afecta la capacidad de asistir
- **Rendimiento por Esfuerzo**: Análisis de rendimiento obtenido por unidad de cansancio
- **Partidos Exigentes Bien Rendidos**: Partidos difíciles con buen rendimiento
- **Partidos Fáciles Mal Rendidos**: Partidos fáciles con bajo rendimiento

### Análisis de Estados Físicos y Mentales

- **Rendimiento por Nivel de Cansancio**: Bajo (1-3), Medio (4-7), Alto (8-10)
- **Goles con Cansancio Alto vs Bajo**: Comparación usando el promedio como referencia
- **Partidos con Cansancio Alto**: Total de partidos con nivel mayor a 7
- **Caída de Rendimiento por Cansancio Acumulado**: Comparación entre partidos recientes y antiguos
- **Rendimiento por Estado de Ánimo**: Bajo (1-3), Medio (4-7), Alto (8-10)
- **Goles por Estado de Ánimo**: Producción según estado emocional
- **Asistencias por Estado de Ánimo**: Análisis de asistencias según ánimo
- **Estado de Ánimo Ideal para Jugar**: Nivel que produce el mejor rendimiento

### Estadísticas por Clima y Día de la Semana

- **Rendimiento Promedio por Clima**: Análisis según condiciones climáticas
- **Goles por Clima**: Total de goles en diferentes climas
- **Asistencias por Clima**: Asistencias según clima
- **Clima Mejor/Peor Rendimiento**: Identificación de condiciones óptimas
- **Mejor/Peor Día de la Semana**: Día con mejor y peor rendimiento
- **Goles Promedio por Día**: Producción por día de la semana
- **Asistencias Promedio por Día**: Asistencias por día
- **Rendimiento Promedio por Día**: Rendimiento general por día

### Récords y Rankings

Sistema completo de récords históricos:

- **Récords Individuales**: Máximo de goles y asistencias en un partido
- **Combinaciones Óptimas**: Mejor y peor combinación cancha + camiseta
- **Temporadas**: Mejor y peor temporada por rendimiento promedio
- **Rendimiento Extremo**: Partidos con mejor y peor rendimiento general
- **Combinaciones**: Partidos con mejor combinación de goles + asistencias
- **Partidos Especiales**: Sin goles, sin asistencias, rachas goleadoras
- **Rachas**: Mejor racha goleadora y peor racha (sin goles)

### Estadísticas de Lesiones

Análisis completo de lesiones:

- **Total de Lesiones**: Número total de incidentes médicos
- **Lesiones por Tipo**: Clasificación por categorías diagnósticas
- **Lesiones por Camiseta**: Distribución por jugador/camiseta
- **Lesiones por Mes**: Análisis temporal mensual
- **Mes con Más Lesiones**: Identificación del período de mayor riesgo
- **Tiempo Promedio entre Lesiones**: Cálculo de intervalos
- **Rendimiento Antes/Después de Lesiones**: Comparación de métricas de producción

![Estadísticas](images/menuestadisticas.png)

## Logros

Selecciona "9" en el menú principal para acceder al sistema de logros y badges. Los logros están organizados por categorías y niveles de dificultad.

### Categorías de Logros

- **Goles**: Novato (10), Promedio (25), Experto (50), Maestro (100), Leyenda (200)
- **Asistencias**: Novato (5), Promedio (15), Experto (30), Maestro (60), Leyenda (120)
- **Partidos**: Novato (10), Promedio (25), Experto (50), Maestro (100), Leyenda (200)
- **Contribuciones** (Goles + Asistencias): Novato (15), Promedio (40), Experto (80), Maestro (160), Leyenda (320)
- **Victorias**: Novato (5), Promedio (15), Experto (30), Maestro (60), Leyenda (120)
- **Rendimiento**: Novato (7.0), Promedio (7.5), Experto (8.0), Maestro (8.5), Leyenda (9.0)
- **Logros Especiales**: Hat-tricks, Poker de asistencias, Rendimiento perfecto, Ánimo perfecto

### Ver Todos los Logros

1. Selecciona "9" en el menú principal
2. Elige "1" para ver todos los logros
3. Se mostrarán todos los logros con su progreso actual

### Ver Logros Completados

1. Selecciona "9" en el menú principal
2. Elige "2" para ver logros completados
3. Se mostrarán solo los logros que has alcanzado

### Ver Logros en Progreso

1. Selecciona "9" en el menú principal
2. Elige "3" para ver logros en progreso
3. Se mostrarán los logros que estás cerca de completar

### Ver Logros No Completados

1. Selecciona "9" en el menú principal
2. Elige "4" para ver logros no completados
3. Se mostrará el listado pendiente con su progreso

![Logros](images/menulogros.png)

## Gestión Financiera

Selecciona "10" en el menú principal para acceder al módulo de gestión financiera del equipo.

### Qué puedes hacer en Financiamiento

- **1. Agregar Transacción**: registrar ingresos y gastos.
- **2. Listar Transacciones**: ver historial financiero.
- **3. Modificar Transacción**: editar movimientos.
- **4. Eliminar Transacción**: borrar movimientos.
- **5. Ver Resumen Financiero**: ver consolidado de ingresos/gastos.
- **6. Balance General de Gastos**: analizar gastos por categoría.
- **7. Exportar Datos**: exportar módulo financiero.
- **8. Presupuestos Mensuales**: gestionar topes y seguimiento mensual.

### Agregar Transacción Financiera

1. Selecciona "10" en el menú principal
2. Elige "1" para agregar una nueva transacción
3. Selecciona el tipo:
   - **Ingreso**: Cuotas, sponsors, premios, etc.
   - **Gasto**: Diversos tipos de gastos
4. Elige la categoría:
   - Transporte
   - Equipamiento
   - Cuotas
   - Torneos
   - Arbitraje
   - Canchas
   - Medicina
   - Otros
5. Ingresa la descripción detallada
6. Especifica el monto
7. Indica el item específico si aplica

### Listar Transacciones

1. Selecciona "10" en el menú principal
2. Elige "2" para ver todas las transacciones financieras
3. Se mostrarán ordenadas por fecha

### Modificar Transacción

1. Selecciona "10" en el menú principal
2. Elige "3" para modificar una transacción existente
3. Ingresa el ID de la transacción a modificar
4. Actualiza los datos necesarios

### Eliminar Transacción

1. Selecciona "10" en el menú principal
2. Elige "4" para eliminar una transacción
3. Ingresa el ID de la transacción
4. Confirma la eliminación

### Ver Resumen Financiero

1. Selecciona "10" en el menú principal
2. Elige "5" para ver un resumen completo
3. Se mostrará:
   - Total de ingresos
   - Total de gastos
   - Balance actual
   - Transacciones recientes

### Ver Balance de Gastos

1. Selecciona "10" en el menú principal
2. Elige "6" para analizar el balance por categorías
3. Se mostrará el desglose de gastos por tipo

### Exportar Datos Financieros

1. Selecciona "10" en el menú principal
2. Elige "7" para exportar las transacciones financieras
3. Selecciona el formato deseado (CSV, JSON, HTML, TXT)

### Presupuestos Mensuales

1. Selecciona "10" en el menú principal
2. Elige "8" para abrir el menú de presupuestos mensuales
3. Configura límites y seguimiento por mes

![Gestión financiera](images/menufinanciamiento.png)

## Gestión de Torneos

Selecciona "11" en el menú principal para acceder al menú de gestión de torneos.

### Qué puedes hacer en Torneos

- **1. Crear Torneo**: crear una nueva competencia.
- **2. Listar Torneos**: consultar torneos cargados.
- **3. Modificar Torneo**: ajustar configuración.
- **4. Eliminar Torneo**: eliminar torneo existente.

### Crear un Torneo

1. Selecciona "11" en el menú principal
2. Elige "1" para crear un nuevo torneo
3. Ingresa el nombre del torneo
4. Selecciona si tiene equipo fijo
5. Elige el tipo de torneo:
   - Ida y Vuelta
   - Solo Ida
   - Eliminación Directa
   - Grupos y Eliminación
6. Selecciona el formato:
   - Round Robin
   - Grupos con Final
   - Copa Simple
   - Eliminación Directa por Fases
7. Especifica la cantidad de equipos participantes

### Listar Torneos

1. Selecciona "11" en el menú principal
2. Elige "2" para listar todos los torneos
3. Se mostrarán con su estado actual

### Modificar un Torneo

1. Selecciona "11" en el menú principal
2. Elige "3" para modificar un torneo
3. Ingresa el ID del torneo a modificar
4. Actualiza la configuración del torneo

### Eliminar un Torneo

1. Selecciona "11" en el menú principal
2. Elige "4" para eliminar un torneo
3. Ingresa el ID del torneo a eliminar
4. Confirma la eliminación

![Gestión de torneos](images/menutorneos.png)

## Gestión de Temporadas

Selecciona "12" en el menú principal para acceder al sistema de gestión de temporadas y ciclos deportivos.

### Qué puedes hacer en Temporadas

- **1. Crear Temporada**: crear una temporada con fechas y estado.
- **2. Listar Temporadas**: ver temporadas registradas.
- **3. Modificar Temporada**: editar campos de una temporada.
- **4. Eliminar Temporada**: borrar una temporada.
- **5. Administrar Temporada**: abrir funciones avanzadas por temporada.
- **6. Comparar Temporadas**: comparar métricas entre dos temporadas.

### Crear una Temporada

1. Selecciona "12" en el menú principal
2. Elige "1" para crear una nueva temporada
3. Ingresa el nombre de la temporada (ej: "Temporada 2026")
4. Especifica el año
5. Ingresa la fecha de inicio (formato YYYY-MM-DD)
6. Ingresa la fecha de fin (formato YYYY-MM-DD)
7. Selecciona el estado:
   - Planificada
   - Activa
   - Finalizada
8. Agrega una descripción (opcional)
9. Se crearán automáticamente las fases por defecto:
   - Pretemporada
   - Temporada Regular
   - Posttemporada

### Listar Temporadas

1. Selecciona "12" en el menú principal
2. Elige "2" para listar todas las temporadas
3. Se mostrarán con sus fechas y estado

### Modificar una Temporada

1. Selecciona "12" en el menú principal
2. Elige "3" para modificar una temporada
3. Ingresa el ID de la temporada a modificar
4. Actualiza los datos necesarios

### Eliminar una Temporada

1. Selecciona "12" en el menú principal
2. Elige "4" para eliminar una temporada
3. Ingresa el ID de la temporada
4. Confirma la eliminación

### Administrar una Temporada

1. Selecciona "12" en el menú principal
2. Elige "5" para administrar una temporada
3. Selecciona la temporada a administrar
4. Accede a funciones avanzadas:
   - **Ver Dashboard**: Vista general de la temporada
   - **Asociar Torneos**: Vincular torneos a la temporada
   - **Ver Estadísticas de Fatiga**: Análisis de cansancio acumulado
   - **Ver Evolución de Equipos**: Tendencias de rendimiento
   - **Generar Resumen**: Resumen automático de la temporada
   - **Comparar Temporadas**: Comparación entre diferentes temporadas
   - **Ver Resúmenes Mensuales**: Análisis mes a mes
   - **Exportar Resumen**: Guardar resumen en archivo

### Comparar Temporadas

1. Selecciona "12" en el menú principal
2. Elige "6" para comparar dos temporadas
3. Selecciona los IDs a comparar para ver diferencias

### Funcionalidades Avanzadas de Temporada

#### Dashboard de Temporada

Muestra información completa:
- Total de partidos jugados
- Goles totales
- Promedio de goles por partido
- Equipo campeón
- Mejor goleador
- Total de lesiones
- Fatiga acumulada de equipos
- Evolución de rendimiento

#### Estadísticas de Fatiga

Análisis de cansancio:
- Fatiga acumulada por equipo
- Fatiga acumulada por jugador
- Partidos jugados
- Rendimiento promedio
- Lesiones acumuladas
- Minutos jugados totales

#### Resúmenes Mensuales

Análisis mensual automático:
- Total de partidos del mes
- Goles del mes
- Promedio de goles por partido
- Partidos ganados/empatados/perdidos
- Total de lesiones
- Gastos e ingresos del mes
- Mejor y peor equipo del mes

![Gestión de temporadas](images/menutemporadas.png)

## Análisis de Rendimiento

Selecciona "13" en el menú principal para ver el análisis de rendimiento.
Desde este menú puedes acceder a **Análisis Básico**, **Comparador Avanzado**, **Análisis Táctico (Diagramas)**, **Entrenador IA** y **Química Entre Jugadores**.

### Qué puedes hacer en Análisis

- **1. Análisis Básico**: revisar indicadores clave de forma directa.
- **2. Comparador Avanzado**: comparar periodos, camisetas y contexto.
- **3. Análisis Táctico (Diagramas)**: crear y consultar esquemas tácticos.
- **4. Entrenador Virtual (IA)**: recibir consejos inteligentes.
- **5. Química Entre Jugadores**: medir sinergia y rendimiento combinado.

### Funcionalidades del Análisis

- **Comparación Últimos 5 Partidos**: Compara el rendimiento reciente con promedios generales
- **Comparador Avanzado**: Comparaciones entre camisetas, torneos, periodos y condiciones
- **Cálculo de Rachas**: Determina la mejor racha de victorias y peor racha de derrotas
- **Análisis Motivacional**: Mensajes personalizados basados en el rendimiento
- **Visualización de Últimos Partidos**: Resumen de los 5 partidos más recientes con detalles clave

### Métricas Analizadas

- Goles promedio
- Asistencias promedio
- Rendimiento general
- Nivel de cansancio
- Estado de ánimo
- Diferencias con respecto al promedio histórico

### Química Entre Jugadores

Dentro de **Análisis** (opción 13), puedes entrar a **Química Entre Jugadores** para:

- Ver la mejor combinación de jugadores por winrate
- Registrar estadísticas manuales por partido (goles, asistencias, posición y comentario)
- Listar, editar y eliminar registros de química

En la selección de partido para química:

- Los partidos se muestran del más reciente al más antiguo
- Puedes ingresar `0` para cancelar la operación

![Análisis y comparador](images/menuanalisis.png)

## Bienestar

Selecciona "14" en el menú principal para acceder a las herramientas de bienestar.

### Qué puedes hacer en Bienestar

- **1. Planificación Personal**: definir objetivos y organización semanal.
- **2. Mentalidad y Hábitos**: seguimiento de hábitos y disciplina.
- **3. Entrenamiento**: registrar sesiones y avances.
- **4. Alimentación**: controlar hábitos nutricionales.
- **5. Asistente Entrenamientos Personalizados**: recibir sugerencias personalizadas.
- **6. Mental**: registrar bienestar mental.
- **7. Informe Personal Mensual (PDF)**: generar informe consolidado.
- **8. Salud**: registrar y revisar estado de salud.

### Funcionalidades de Bienestar

- **Planificación Personal**: Objetivos, rutinas y seguimiento
- **Mentalidad y Hábitos**: Registro de hábitos diarios
- **Entrenamiento**: Planes y controles de práctica
- **Alimentación**: Seguimiento y recomendaciones
- **Mental**: Sesiones y seguimiento mental
- **Informe Personal Mensual (PDF)**: Resumen automático
- **Salud**: Perfil de salud y controles médicos

![Bienestar](images/menubienestar.png)

## Carrera Futbolística

Selecciona "15" en el menú principal para acceder al módulo de carrera futbolística.

### Qué puedes hacer en Carrera Futbolística

- **1. Carrera Futbolística**: gestionar objetivos, hitos y progreso.
- **2. Tu Historia Futbolística**: consultar narrativa/historial personal.
- **3. Resumen General de Carrera**: ver consolidado de tu evolución.

Este módulo te ayuda a llevar seguimiento longitudinal de tu desarrollo deportivo con foco en trayectoria y metas personales.

![Carrera futbolística](images/menucarrerafutbolistica.png)

## Reproductor de Música

Selecciona "19" en el menú principal para acceder al reproductor de música integrado.

El reproductor utiliza la biblioteca **miniaudio** y detecta automáticamente los archivos de audio de la carpeta `Musica/` ubicada junto al ejecutable. Soporta los formatos `.mp3`, `.wav`, `.flac` y `.ogg`.

### Qué puedes hacer en Música

- Reproducir, pausar, detener y navegar pistas.
- Gestionar volumen con paso configurable.
- Aplicar ecualizador de 3 bandas.
- Crear/cargar/eliminar playlists.
- Buscar y filtrar pistas por nombre.
- Usar temporizador de apagado y saltos temporales.
- Ver información técnica y renombrar pistas.
- Exportar catálogo como playlist.

![Reproductor de música](images/menumusica.png)

### Configuración Inicial

1. Crea la carpeta `Musica/` junto al ejecutable (el programa la crea automáticamente si no existe).
2. Copia tus archivos de audio (`.mp3`, `.wav`, `.flac`, `.ogg`) dentro de esa carpeta.
3. Abre el reproductor desde el menú principal (opción **19**).
4. La lista de pistas se cargará automáticamente.

### Controles del Reproductor

| Opción | Acción |
|--------|--------|
| **1** | Reproducir / Pausar la pista actual |
| **2** | Detener (rebobina al inicio) |
| **3** | Pista anterior |
| **4** | Pista siguiente |
| **5** | Seleccionar una pista de la lista |
| **6** | Subir volumen (paso configurable, ver [24]) |
| **7** | Bajar volumen (paso configurable, ver [24]) |
| **8** | Cambiar modo de repetición / shuffle |
| **9** | Actualizar lista (reescanear carpeta `Musica/`) |
| **10** | Agregar canción a la carpeta |
| **11** | Eliminar canción de la carpeta |
| **12** | Ecualizador de 3 bandas |
| **13** | Playlists |
| **14** | Música al iniciar (ON/OFF) |
| **15** | Buscar pista por nombre (activa filtro) |
| **16** | Retroceder 10 segundos |
| **17** | Avanzar 10 segundos |
| **18** | Limpiar filtro de búsqueda |
| **19** | Temporizador de apagado (sleep timer) |
| **20** | Saltar a tiempo exacto (formato MM:SS) |
| **21** | Información técnica de la pista activa |
| **22** | Renombrar pista |
| **23** | Exportar catálogo como playlist |
| **24** | Configurar paso de volumen |
| **0** | Volver al menú principal |

### Interfaz Visual

El reproductor muestra en pantalla:

- **Pista actual**: nombre del archivo y posición en la lista (ej. `[2/8]`).
- **Barra de progreso**: tiempo actual y duración total (`mm:ss / mm:ss`) con tiempo restante.
- **Barra de volumen**: nivel visual de 0 % a 100 %.
- **Estado**: `REPRODUCIENDO`, `PAUSADO` o `DETENIDO`.
- **Modo de repetición**: Sin repetición, Repetir pista, Repetir lista o Aleatorio.
- **Filtro activo**: muestra el término de búsqueda si hay un filtro aplicado.
- **Estado del ecualizador**: activo/desactivado con niveles de cada banda en dB.
- **Inicio automático**: indica si la música arrancará al abrir la aplicación.

### Modos de Repetición

| Modo | Comportamiento |
|------|----------------|
| **Sin repetición** | Avanza pista a pista; al terminar la última se detiene |
| **Repetir pista** | Repite la misma pista indefinidamente |
| **Repetir lista** | Al terminar la última vuelve a la primera |
| **Aleatorio (shuffle)** | Elige la siguiente pista de forma aleatoria |

Pulsa la opción **8** repetidamente para ciclar entre los cuatro modos.  
En modo **Aleatorio**, la opción **[3] Pista anterior** retrocede por el historial de pistas ya escuchadas.

### Ecualizador (3 Bandas)

Accede con la opción **12**.

| Banda | Frecuencia central | Rango de ajuste |
|-------|-------------------|------------------|
| **Graves** | 200 Hz | −12 dB a +12 dB |
| **Medios** | 1 000 Hz | −12 dB a +12 dB |
| **Agudos** | 8 000 Hz | −12 dB a +12 dB |

- Cada pulsación ajusta **3 dB** hacia arriba o hacia abajo.
- El ecualizador puede activarse o desactivarse sin perder los ajustes.
- Los cambios se aplican en tiempo real.

### Playlists

Accede con la opción **13**.

- **Crear playlist**: asigna un nombre y añade las pistas deseadas.
- **Cargar playlist**: activa una playlist guardada como lista de reproducción.
- **Eliminar playlist**: borra una playlist guardada.

### Exportar Catálogo como Playlist (opción 23)

Genera un archivo `.txt` en `Musica/` con la lista de pistas del catálogo actual.

1. Selecciona **23**.
2. Si hay un filtro de búsqueda activo, solo se exportarán las pistas que coincidan.
3. Ingresa el nombre de la playlist (sin extensión).
4. El archivo se guarda en la carpeta `Musica/` y puede cargarse desde **Playlists**.

### Búsqueda y Filtro (opciones 15 y 18)

- **[15] Buscar pista**: ingresa una parte del nombre; el reproductor filtra la lista y solo muestra coincidencias.
- El filtro permanece activo hasta que lo limpies con **[18]** o dejes el campo vacío al buscar.
- La lista principal, la selección de pista ([5]) y la exportación de catálogo ([23]) respetan el filtro activo.

### Navegación Temporal (opciones 16, 17 y 20)

- **[16] Retroceder 10 s**: salta 10 segundos hacia atrás dentro de la pista actual.
- **[17] Avanzar 10 s**: salta 10 segundos hacia adelante.
- **[20] Saltar a tiempo exacto**: ingresa el momento en formato `MM:SS` (ej. `1:20` para 1 min 20 s) o solo segundos (ej. `80`). El reproductor se posiciona exactamente en ese instante.

### Temporizador de Apagado — Sleep Timer (opción 19)

1. Selecciona **19**.
2. Ingresa los minutos tras los cuales deseas que se detenga la reproducción.
3. El countdown corre en segundo plano; cuando se agota, la música se detiene con fade-out suave.
4. Selecciona **19** de nuevo y pon `0` para cancelar el temporizador.

### Información de Pista Activa (opción 21)

Muestra los datos técnicos de la pista que está cargada:

- Nombre del archivo.
- Duración total (`MM:SS` y frames PCM).
- Formato (extensión del archivo).
- Sample rate en Hz.
- Número de canales.
- Ruta completa en disco.

### Renombrar Pista (opción 22)

1. Selecciona **22**.
2. Elige el número de la pista de la lista (la pista en reproducción no puede renombrarse; pausa primero).
3. Ingresa el nuevo nombre **incluyendo la extensión** (ej. `Mi Cancion.mp3`).
4. El archivo se renombra en disco y la lista se actualiza de inmediato.

### Configurar Paso de Volumen (opción 24)

Permite elegir cuánto sube o baja el volumen con las opciones **[6]** y **[7]**.

| Opción | Paso | Descripción |
|--------|------|-------------|
| **1** | 1 % | Ajuste fino |
| **2** | 5 % | Ajuste moderado |
| **3** | 10 % | Ajuste rápido (valor por defecto) |
| **4** | 20 % | Ajuste máximo |

El paso elegido se guarda en la base de datos y persiste entre sesiones. Las etiquetas de `[6]` y `[7]` en el menú muestran dinámicamente el porcentaje activo.

### Reanudar Posición Automáticamente

Al salir del reproductor (opción **0**), el programa guarda automáticamente:
- El nombre de la pista que estaba activa.
- El instante exacto (frame PCM) donde se encontraba.

La próxima vez que entres al reproductor, la pista se cargará y el cursor se posicionará donde lo dejaste. Esta función se activa una sola vez por sesión.

### Música al Iniciar

La opción **14** activa o desactiva la reproducción automática al arrancar MiFutbolC.

- Cuando está **activada**, la primera pista comenzará a reproducirse al iniciar la aplicación.
- La preferencia se guarda en la base de datos y persiste entre sesiones.
- También puede configurarse desde **Ajustes**.

### Gestión de Archivos de Audio

#### Agregar una canción (opción 10)

1. Selecciona **10** en el reproductor.
2. Ingresa la ruta completa del archivo de audio que deseas agregar.
3. El programa copiará el archivo a la carpeta `Musica/`.
4. La lista se actualizará automáticamente.

#### Eliminar una canción (opción 11)

1. Selecciona **11** en el reproductor.
2. Elige la pista de la lista numerada.
3. Confirma la eliminación.

> ⚠️ Esta acción **elimina el archivo físicamente** de la carpeta `Musica/`. No se puede deshacer.

#### Actualizar lista (opción 9)

Si agregas o eliminas archivos de audio manualmente desde el explorador, usa la opción **9** para que el reproductor reescanee la carpeta.

### Preguntas Frecuentes del Reproductor

**¿Qué formatos de audio soporta?**  
`.mp3`, `.wav`, `.flac` y `.ogg` (cuando el compilador incluye soporte Vorbis). Otros formatos no serán detectados aunque se copien en la carpeta.

**¿Dónde se almacenan las canciones?**  
En la subcarpeta `Musica/` junto al ejecutable del programa.

**¿El volumen se guarda entre sesiones?**  
Sí. El nivel de volumen y el paso de volumen configurado persisten en la base de datos.

**¿La música continúa al navegar por otros menús?**  
Sí, la reproducción continúa en segundo plano mientras usas el resto de la aplicación.

**¿Cómo retomo la pista donde la dejé?**  
Automáticamente: al volver al reproductor tras salir con `[0]`, el sistema restaura la pista y el instante exacto de la sesión anterior.

## Entrenador IA

Selecciona "13" en el menú principal, luego **Entrenador IA** (opción 4) dentro de Análisis.

### Funcionalidades del Entrenador IA

#### Consejos Actuales

1. Selecciona "13" en el menú principal
2. Elige "4" para entrar a Entrenador IA
3. Elige "1" para ver consejos actuales
4. El sistema evaluará tu estado actual:
   - Rendimiento promedio
   - Cansancio promedio
   - Estado de ánimo promedio
   - Partidos consecutivos
   - Riesgo de lesión
   - Derrotas consecutivas
   - Días de descanso
5. Recibirás consejos personalizados por categoría:
   - **Físico**: Recomendaciones sobre cansancio y recuperación
   - **Mental**: Consejos sobre estado de ánimo
   - **Deportivo**: Sugerencias tácticas y de rendimiento
   - **Salud**: Prevención de lesiones
   - **Gestión**: Administración del equipo

#### Niveles de Consejos

- **Información**: Consejos generales
- **Advertencia**: Situaciones que requieren atención
- **Crítico**: Situaciones urgentes que necesitan acción inmediata

#### Historial de Consejos

1. Selecciona "13" en el menú principal
2. Elige "4" para entrar a Entrenador IA
3. Elige "2" para ver historial de consejos
4. Se mostrarán todos los consejos anteriores
5. Podrás ver si seguiste o no cada consejo

#### Evaluar Decisión Pasada

1. Selecciona "13" en el menú principal
2. Elige "4" para entrar a Entrenador IA
3. Elige "3" para evaluar decisiones pasadas
4. El sistema analizará el impacto de seguir o ignorar consejos

#### Configurar Nivel de Intervención

1. Selecciona "13" en el menú principal
2. Elige "4" para entrar a Entrenador IA
3. Elige "4" para configurar el nivel de intervención
4. Ajusta qué tan frecuentes y detallados quieres los consejos

#### Activación Automática

El Entrenador IA se activa automáticamente:
- Antes de un partido importante
- Antes de un torneo
- Al revisar estadísticas
- Cuando detecta situaciones de riesgo

## Recordatorios

Selecciona "16" en el menú principal para acceder al módulo de recordatorios.

### Qué puedes hacer en Recordatorios

- **1. Listar recordatorios**: ver todos los recordatorios.
- **2. Agregar recordatorio**: crear nuevos avisos.
- **3. Editar recordatorio**: modificar recordatorios existentes.
- **4. Eliminar recordatorio**: quitar recordatorios.
- **5. Filtrar por temática**: buscar por categoría.
- **6. Exportar recordatorios**: exportar datos del módulo.
- **7. Importar recordatorios**: importar datos previamente exportados.
- **8. Agenda**: revisar agenda de próximos/pasados.

### Funcionalidades de Recordatorios

- **Listar recordatorios**: Visualización completa de recordatorios guardados
- **Agregar/Editar/Eliminar**: Gestión completa de entradas
- **Filtrar por temática**: Consulta por categoría
- **Exportar e importar**: Portabilidad de recordatorios
- **Agenda**: Vista de eventos próximos y eventos pasados

![Recordatorios](images/menurecordatorios.png)

## Colecciones e Inventario

Selecciona "17" en el menú principal para abrir el módulo de colecciones e inventario.

### Qué puedes hacer en Colecciones e Inventario

- **1. Crear item de inventario**: registrar un nuevo item.
- **2. Listar inventario**: ver inventario completo.
- **3. Sincronizar camisetas al inventario**: importar camisetas como items.
- **4. Crear colección**: crear una nueva colección.
- **5. Listar colecciones**: consultar colecciones existentes.
- **6. Agregar item a colección**: vincular item a una colección.
- **7. Quitar item de colección**: desvincular item.
- **8. Ver items por colección**: listar contenido por colección.
- **9. Filtrar y buscar inventario**: localizar items rápidamente.
- **10. Exportar backup JSON**: respaldo completo del módulo.
- **11. Importar backup JSON**: restaurar respaldo.

### Funcionalidades de Colecciones e Inventario

- **Inventario**: Crear items, listar y filtrar inventario
- **Sincronización**: Sincronizar camisetas al inventario
- **Colecciones**: Crear colecciones y listar su contenido
- **Vinculación de ítems**: Agregar o quitar ítems de una colección
- **Backups JSON**: Exportar e importar respaldo del módulo

![Colecciones e inventario](images/menucolecciones.png)

## Exportar Datos

Selecciona "18" en el menú principal (Ajustes) y luego **Exportar** (opción 8) para acceder al menú de exportación.

### Opciones de Exportación

1. **Camisetas** - Exportar datos de camisetas
2. **Partidos** - Exportar datos de partidos (con submenú)
3. **Lesiones** - Exportar datos de lesiones
4. **Estadísticas** - Exportar estadísticas del módulo
5. **Análisis** - Exportar análisis de rendimiento
6. **Estadísticas Generales** - Submenú de estadísticas globales
7. **Análisis Avanzado** - Exportación mejorada con análisis integrado
8. **Base de Datos** - Exportar copia de la base de datos
9. **Todo** - Exportar todos los datos
10. **Todo JSON** - Exportación completa en JSON
11. **Todo CSV** - Exportación completa en CSV
12. **Informe Total PDF** - Reporte integral en PDF

### Formatos Disponibles

Para cada módulo puedes elegir el formato:
- **CSV**: Valores separados por comas (ideal para Excel)
- **TXT**: Texto plano formateado
- **JSON**: Formato estructurado (ideal para integración)
- **HTML**: Página web con tablas

El informe PDF total incluye secciones adicionales con resúmenes financieros, ranking de canchas,
partidos por clima, lesiones por tipo/estado, historial de rachas y distribución de estado de ánimo/cansancio.

### Submenú de Exportar Partidos

- **Todos los Partidos**: Exportar todos los partidos registrados
- **Partido con Más Goles**: Exportar el partido con más goles
- **Partido con Más Asistencias**: Exportar el partido con más asistencias
- **Partido Menos Goles Reciente**: Exportar el partido más reciente con menos goles
- **Partido Menos Asistencias Reciente**: Exportar el partido más reciente con menos asistencias

### Submenú de Exportar Estadísticas

- **Estadísticas Generales**: Exportar estadísticas generales completas
- **Estadísticas Por Mes**: Exportar estadísticas por mes
- **Estadísticas Por Año**: Exportar estadísticas por año
- **Récords & Rankings**: Exportar récords y rankings

### Exportación Mejorada (Análisis Avanzado)

La opción "Análisis Avanzado" proporciona exportación con análisis integrado:

#### Camisetas con Análisis Avanzado
- Eficiencia de goles/asistencias
- Porcentaje de victorias
- Análisis de lesiones
- Métricas de rendimiento
- Tendencias de uso

#### Lesiones con Análisis de Impacto
- Evaluación de gravedad de lesiones
- Comparación de rendimiento antes/después
- Identificación de patrones de lesiones
- Análisis de recuperación

#### Todo con Análisis Avanzado
- Combina todas las funcionalidades mejoradas
- Análisis completo del sistema
- Recomendaciones basadas en datos

### Ubicación de Archivos Exportados

- **Windows**: `%USERPROFILE%\Documents\MiFutbolC\Exportaciones`
- **Linux/macOS**: `./exportaciones`

Los archivos se guardan con nombres descriptivos como:
- `camisetas.csv`
- `partidos.json`
- `estadisticas.html`
- `lesiones.txt`

> Nota: no hay captura dedicada del submenú de exportación en la carpeta `images/`; el acceso visual está integrado en el menú de **Ajustes**.

## Importar Datos

Selecciona "18" en el menú principal (Ajustes) y luego **Importar** (opción 9) para acceder a la importación de datos.

### Preparación para Importar

1. Los archivos pueden estar en formato JSON, TXT, CSV o HTML
2. Deben ubicarse en el directorio de importaciones:
   - **Windows**: `%USERPROFILE%\Documents\MiFutbolC\Importaciones`
   - **Linux/macOS**: `./importaciones`
3. Los archivos deben tener los nombres específicos generados por la exportación

### Proceso de Importación

1. Selecciona "18" en el menú principal y luego **Importar**
2. Elige una opción del menú de importación:
   - "1" Importar desde JSON
   - "2" Importar desde TXT
   - "3" Importar desde CSV
   - "4" Importar desde HTML
   - "5" Todo JSON rápido
   - "6" Todo CSV rápido
   - "7" Importar base de datos
4. El sistema validará la estructura del archivo
5. Se verificará que los datos sean correctos
6. Los datos se insertarán en la base de datos
7. Recibirás un resumen de la importación:
   - Registros importados exitosamente
   - Errores encontrados (si los hay)
   - Advertencias sobre datos duplicados

### Validaciones Realizadas

- Estructura de archivo correcta
- Campos requeridos presentes
- Tipos de datos válidos
- Referencias a IDs existentes
- Duplicados (se evita la importación de duplicados)

### Manejo de Errores

Si hay errores durante la importación:
- Se mostrará un mensaje detallado del error
- Los datos no se importarán parcialmente
- La base de datos permanecerá intacta
- Podrás corregir el archivo y reintentar

> Nota: no hay captura dedicada del submenú de importación en la carpeta `images/`; el acceso visual está integrado en el menú de **Ajustes**.

## Configuración (Ajustes)

Selecciona "18" en el menú principal para acceder al menú de configuración del sistema.

### Qué puedes hacer en Ajustes

- **1. Tema de Interfaz**: cambiar apariencia.
- **2. Idioma**: alternar idioma.
- **3. Accesibilidad**: ajustar legibilidad.
- **4. Usuario**: gestionar perfil y contraseña.
- **5. Ver Configuración Actual**: revisar estado actual.
- **6. Restablecer a Valores por Defecto**: volver a configuración base.
- **7. Modo**: definir modo de uso.
- **8. Exportar**: abrir submenú de exportación.
- **9. Importar**: abrir submenú de importación.
- **10. Búsqueda Global**: buscar rápidamente dentro del sistema.
- **11. Actualizar**: ejecutar flujo de actualización.
- **12. Música al iniciar**: activar/desactivar auto-reproducción al iniciar app.

### Cambiar Tema de Interfaz

1. Selecciona "18" en el menú principal
2. Elige "1" para cambiar el tema
3. Selecciona uno de los temas disponibles:
   - **Claro**: Fondo claro, texto oscuro
   - **Oscuro**: Fondo oscuro, texto claro
   - **Azul**: Tonos azules
   - **Verde**: Tonos verdes
   - **Rojo**: Tonos rojos
   - **Púrpura**: Tonos púrpuras
   - **Clásico**: Estilo retro
   - **Alto Contraste**: Máxima legibilidad
4. El tema se aplicará inmediatamente
5. La configuración se guardará para futuras sesiones

### Cambiar Idioma

1. Selecciona "18" en el menú principal
2. Elige "2" para cambiar el idioma
3. Selecciona entre:
   - **Español**: Idioma por defecto
   - **Inglés**: English language
4. El idioma se aplicará inmediatamente
5. Todos los menús y mensajes cambiarán al nuevo idioma

### Accesibilidad

1. Selecciona "18" en el menú principal
2. Elige "3" para abrir accesibilidad
3. Ajusta el tamaño del texto o activa alto contraste

### Gestión de Usuario y Seguridad

1. Selecciona "18" en el menú principal
2. Elige "4" para abrir **Usuario**
3. Desde este menú puedes:
   - Mostrar nombre actual
   - Editar nombre visible
   - Agregar usuario local
   - Modificar tu contraseña
   - Quitar tu contraseña
   - Eliminar tu cuenta local (irreversible)
4. Los cambios se aplican al perfil activo

### Ver Configuración Actual

1. Selecciona "18" en el menú principal
2. Elige "5" para ver la configuración actual
3. Se mostrará:
   - Tema actual
   - Idioma actual
   - Nombre visible del perfil
   - Ubicación de la base de datos
   - Directorios de exportación e importación

### Restablecer Valores por Defecto

1. Selecciona "18" en el menú principal
2. Elige "6" para restablecer configuración por defecto
3. Confirma la acción
4. Se restaurarán:
   - Tema: Claro
   - Idioma: Español
   - Otras configuraciones a valores iniciales

### Modo de Menú

1. Selecciona "18" en el menú principal
2. Elige "7" para configurar el modo
3. Selecciona modo Simple, Avanzado o Personalizado

### Exportar / Importar desde Ajustes

1. Selecciona "18" en el menú principal
2. Elige "8" para **Exportar** o "9" para **Importar**

### Búsqueda Global

1. Selecciona "18" en el menú principal
2. Elige "10" para abrir la búsqueda global
3. Escribe el término a buscar para localizar contenido entre módulos

### Actualizar aplicación

1. Selecciona "18" en el menú principal
2. Elige "11" para abrir el flujo de actualización
3. En Windows, podrás buscar y ejecutar la actualización

### Música al iniciar desde Ajustes

1. Selecciona "18" en el menú principal
2. Elige "12" para activar o desactivar reproducción automática al iniciar
3. El valor se guarda y persiste entre sesiones

![Menú de ajustes](images/menuajustes.png)

## Consejos de Uso

### Mejores Prácticas

- **Backups Regulares**: Utiliza la función de exportación regularmente para hacer copias de seguridad completas
- **Confirmación de Eliminaciones**: Siempre confirma las operaciones de eliminación para evitar pérdida de datos
- **Análisis Periódico**: Revisa las estadísticas y análisis semanalmente para mejorar el rendimiento
- **Registro Detallado**: Completa todos los campos al registrar partidos para obtener análisis más precisos
- **Uso de Logros**: Completa logros para motivarte a usar la aplicación de manera consistente
- **Entrenador IA**: Presta atención a los consejos críticos del Entrenador IA
- **Gestión Financiera**: Registra todas las transacciones para mantener un control preciso
- **Temporadas**: Organiza tus partidos en temporadas para un mejor seguimiento histórico

### Optimización del Rendimiento

- **Base de Datos**: La base de datos SQLite es eficiente, pero considera hacer backups antes de importaciones grandes
- **Exportaciones**: Usa formatos CSV para análisis en hojas de cálculo, JSON para backups completos
- **Filtros**: Usa las opciones de filtrado en estadísticas para análisis más específicos

## Solución de Problemas

### El programa no se ejecuta

**Problema**: El ejecutable no inicia o muestra error inmediatamente

**Soluciones**:
- Verifica que el archivo ejecutable existe en `bin/Debug/MiFutbolC.exe` (Windows) o `./MiFutbolC` (Linux/macOS)
- Asegúrate de tener permisos para ejecutar archivos en el directorio
- En Linux/macOS, verifica permisos: `chmod +x MiFutbolC`
- Verifica que todas las bibliotecas necesarias estén presentes (SQLite3, cJSON)

### Error al conectar con la base de datos

**Problema**: Mensaje de error relacionado con la base de datos

**Soluciones**:
- Verifica que el directorio de datos existe y tienes permisos de escritura:
  - Windows: `%LOCALAPPDATA%\MiFutbolC\data\`
  - Linux/macOS: `./data/`
- El programa creará automáticamente la base de datos `mifutbol_<usuario>.db` del perfil activo si no existe
- Verifica también que el archivo de usuarios `users.db` exista y sea accesible
- Si la base de datos está corrupta, renómbrala y el programa creará una nueva
- Verifica espacio disponible en disco

### Datos no se guardan

**Problema**: Los cambios no persisten entre sesiones

**Soluciones**:
- Verifica que no hay errores en la consola al guardar
- Revisa que la base de datos no esté en modo solo lectura
- Comprueba que tienes permisos de escritura en el directorio de datos
- Verifica que no estés ejecutando múltiples instancias del programa

### Caracteres extraños en la consola (Windows)

**Problema**: Caracteres especiales no se muestran correctamente

**Soluciones**:
- El programa configura automáticamente UTF-8
- Si persiste, ejecuta manualmente: `chcp 65001` en la consola antes de ejecutar el programa
- Verifica que tu terminal soporta UTF-8

### Error al exportar datos

**Problema**: La exportación falla o no se encuentran los archivos

**Soluciones**:
- Verifica que el directorio de exportaciones existe:
  - Windows: `%USERPROFILE%\Documents\MiFutbolC\Exportaciones`
  - Linux/macOS: `./exportaciones`
- Asegúrate de tener permisos de escritura en ese directorio
- Verifica espacio disponible en disco
- Comprueba que no haya archivos con el mismo nombre bloqueados por otra aplicación

### Error al importar datos

**Problema**: La importación falla o muestra errores de validación

**Soluciones**:
- Verifica que el archivo esté bien formado (JSON/TXT/CSV/HTML)
- Asegúrate de que el archivo está en el directorio correcto de importaciones
- Comprueba que los datos son válidos (IDs existentes, tipos correctos)
- Revisa el mensaje de error específico para identificar el problema
- Intenta exportar primero para ver el formato correcto esperado

### Problemas de rendimiento

**Problema**: El programa se vuelve lento con muchos datos

**Soluciones**:
- Considera archivar datos antiguos exportándolos y eliminándolos de la base de datos activa
- Cierra otras aplicaciones que puedan estar usando recursos
- Verifica que la base de datos no esté fragmentada (SQLite se optimiza automáticamente)
- En sistemas con muchos datos, considera usar filtros para limitar resultados

### Códigos QR no se generan

**Problema**: Error al generar códigos QR

**Soluciones**:
- Verifica que tienes permisos de escritura en el directorio de exportaciones
- Asegúrate de que los datos del partido/camiseta/temporada existen
- Comprueba espacio disponible en disco
- Verifica que no haya caracteres especiales problemáticos en los datos

## Preguntas Frecuentes (FAQ)

### ¿Puedo usar MiFutbolC en múltiples computadoras?

Sí, puedes exportar todos los datos desde una computadora e importarlos en otra. Usa la opción **Ajustes → Exportar → Todo** en formato JSON para crear un backup completo.

### ¿Los datos se guardan automáticamente?

Sí, todos los cambios se guardan inmediatamente en la base de datos SQLite. No necesitas guardar manualmente.

### ¿Puedo editar la base de datos directamente?

Aunque es posible usar herramientas como DB Browser for SQLite, se recomienda usar solo las funciones del programa para evitar corrupción de datos.

### ¿Cómo hago un backup completo?

1. Ve a Ajustes (opción 18)
2. Entra en **Exportar**
3. Selecciona "Todo" (opción 9)
4. Elige formato JSON
5. Guarda el archivo en un lugar seguro
6. Para restaurar, usa **Ajustes → Importar**

### ¿Puedo usar MiFutbolC para múltiples equipos?

Sí, el sistema soporta múltiples equipos, torneos y temporadas. Puedes gestionar toda una liga o múltiples equipos simultáneamente.

### ¿Puedo personalizar los logros?

Actualmente los logros están predefinidos, pero puedes sugerir nuevos logros para futuras versiones del programa.

### ¿El Entrenador IA aprende de mis decisiones?

Sí, el Entrenador IA mantiene un historial de consejos y evalúa si los seguiste o no, ajustando sus recomendaciones futuras basándose en tu perfil.

## Glosario de Términos

- **Camiseta**: Representa un jugador o equipamiento específico usado en partidos
- **Cancha**: Ubicación donde se juega un partido
- **Partido**: Evento deportivo registrado con estadísticas completas
- **Equipo**: Conjunto de jugadores organizados con formación
- **Torneo**: Competición organizada con múltiples equipos
- **Temporada**: Ciclo deportivo con fechas de inicio y fin
- **Fase**: Período dentro de una temporada (Pretemporada, Regular, Posttemporada)
- **Lesión**: Incidente médico que afecta a un jugador
- **Logro**: Meta alcanzable basada en estadísticas
- **Badge**: Insignia otorgada al completar un logro
- **Rendimiento**: Calificación del desempeño en un partido (1-10)
- **Cansancio**: Nivel de fatiga física (1-10)
- **Estado de Ánimo**: Nivel emocional/mental (1-10)
- **Meta-Análisis**: Análisis estadístico avanzado de múltiples variables
- **Outlier**: Dato atípico que se desvía significativamente del promedio
- **Fixture**: Calendario de partidos de un torneo
- **Dashboard**: Panel de control con información resumida
- **Entrenador IA**: Sistema de inteligencia artificial que proporciona consejos
- **miniaudio**: Biblioteca de audio multiplataforma usada internamente para el reproductor MP3
- **Playlist**: Lista de reproducción personalizada dentro del reproductor de música
- **EQ / Ecualizador**: Herramienta que ajusta el perfil de frecuencias del audio (graves, medios, agudos)
- **Fade**: Transición suave de volumen al comenzar o cambiar pista de audio

## Conclusión

MiFutbolC es una herramienta completa y profesional para el seguimiento y análisis de datos relacionados con el fútbol. Con su interfaz intuitiva, funcionalidades avanzadas y sistema de análisis profundo, te permite gestionar todos los aspectos de tu experiencia futbolística de manera eficiente y organizada.

### Características Destacadas

✅ **Gestión Integral**: Desde equipamiento hasta finanzas  
✅ **Análisis Avanzado**: Estadísticas profesionales y meta-análisis  
✅ **Sistema Inteligente**: Entrenador IA con recomendaciones personalizadas  
✅ **Gamificación**: Logros y badges para mantener la motivación  
✅ **Flexibilidad**: Múltiples formatos de exportación e importación  
✅ **Personalización**: Temas, idiomas y configuraciones adaptables  
✅ **Organización**: Torneos y temporadas completas  
✅ **Reproductor de Música**: MP3 integrado con ecualizador 3 bandas y playlists  

### Próximos Pasos

1. **Explora las funcionalidades**: Prueba cada módulo para familiarizarte
2. **Registra tus datos**: Comienza a ingresar partidos y estadísticas
3. **Analiza tu rendimiento**: Usa las herramientas de análisis regularmente
4. **Sigue los consejos**: Presta atención al Entrenador IA
5. **Completa logros**: Motívate alcanzando metas
6. **Haz backups**: Exporta tus datos regularmente

¡Disfruta usando MiFutbolC y mejora tu gestión deportiva!

---

**Desarrollado por**: Thomas Hamer  
**Versión**: 4.2  
**Última actualización**: 23/04/2026  
**Licencia**: Open Source  

*Manual generado para MiFutbolC - Sistema Integral de Gestión de Fútbol*
