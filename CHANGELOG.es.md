# Changelog

[Português](CHANGELOG.md) · [English](CHANGELOG.en.md) · **Español**

Notas en español de las versiones recientes. Si una versión no aparece aquí, el launcher muestra la nota en inglés.

## v1.4-nx

La mayor actualización desde el lanzamiento: un **renderizador deko3d nativo**, un **launcher y un mod menu
rediseñados**, nuevas opciones de imagen, un sistema de trofeos, un actualizador integrado y soporte para **inglés y
portugués**.

### Rendimiento
- **Renderizador deko3d** (la API nativa de la GPU de la Switch) como opción principal, con menos carga de CPU que
  Vulkan. Vulkan sigue disponible en *Opciones → Imagen → Renderizador*.
- **GPU a 460,8 MHz en modo portátil**: el juego usa el perfil de rendimiento portátil oficial (el mismo que usan los
  juegos comerciales; no es overclock) y lo mantiene durante toda la sesión. En el dock, el sistema queda a 768 MHz.
- **Generación de cuadros (LSFG)** en *God of War I* con deko3d: crea cuadros intermedios para mostrar 60 fps
  con el juego a 30. El flujo de generación ajustable (completo, 1/2, 1/3, 1/4) adapta el costo a la resolución, se ajusta
  automáticamente cuando un tamaño no sirve para la resolución elegida y se pausa automáticamente cuando la GPU no
  da abasto.
- A 720p en portátil, FSR omite el reescalado (ya está casi a 1x) y solo aplica nitidez, ahorrando tiempo de GPU.

### Imagen
- Reescalador **AMD FSR 1.0** (EASU + RCAS), con nitidez de 0 a 10.
- **FXAA**, **filtro anisotrópico** (hasta 16x), **brillo**, **saturación**, **vibrancia** y **debanding**.
- **Resolución de TV separada**: una resolución para portátil y otra para el dock, que se alternan automáticamente.
- **Perfiles rápidos** (Rendimiento, Equilibrado, Calidad) en el launcher y en el mod menu.

### Launcher rediseñado
- Nuevo diseño: tarjetas con la portada de cada juego, un panel con partida guardada, trofeos y extras instalados
  (texturas HD, videos HD), música de fondo y el último juego jugado.
- **Opciones por pestañas** (Imagen, Color, Rendimiento, Juego), con descripción y valor por defecto en cada opción;
  todo se aplica a ambos juegos.
- **Trofeos**: la lista completa de cada juego, con progreso, puntos, filtros (todos, desbloqueados, pendientes) y
  trofeos ocultos sin spoilers.
- **Actualizador integrado**: el launcher busca versiones nuevas en GitHub e instala el launcher, *God of War* y
  *God of War II* de una vez, conservando partidas, opciones, videos y texturas.

### Mod menu rediseñado (R + Plus)
- Nuevo panel por pestañas (**Gráficos, Mods, Trucos, Sistema**) con interruptores, barras de nivel y descripción en
  cada opción.
- Todas las nuevas opciones de imagen y rendimiento cambian con el juego en marcha, sin reiniciar.
- **Trucos**: vida, magia y furia infinitas. **Mods**: texturas HD, barras de vida enemigas y QTE automático.
- Diagnóstico de GPU para registros de rendimiento, con un aviso antes de activarse.
- **Salir del juego** en *Sistema*, con confirmación.

### Idiomas
- **Inglés y portugués (Brasil)** en el launcher y el mod menu. Se elige en *Opciones → Juego → Idioma* o en
  *Sistema → Idioma* dentro de R + Plus; se aplica a ambos juegos.

### Correcciones
- El movimiento diagonal del stick ya no frena a Kratos.
- Los videos HD abren correctamente en la Switch.
- Las texturas HD usan una cola de decodificación fija y una caché más pequeña (menos memoria, menos tirones).
- Los hilos se reutilizan y se comprueba la memoria antes de escanear (#8).
- Corregida la pantalla negra con FSR 1.0.
- Corregida la generación de cuadros que no arrancaba en algunas resoluciones; la espera de VSync ya no cuenta
  como carga de GPU.
- Los cierres inesperados (abort, error de C++ no controlado) ahora cierran el juego limpiamente y registran el
  motivo, en lugar de llegar a la pantalla de error de Atmosphère (2168-0001).
- El actualizador instala el paquete `gow-nx.zip` (launcher + ambos juegos) y ya no ofrece la misma versión una y
  otra vez.
