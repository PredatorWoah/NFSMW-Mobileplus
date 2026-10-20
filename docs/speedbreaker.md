# SpeedBreaker: el tercer motor, para Mali

[speedbreaker/](../speedbreaker/) es otra recompilación de Need for Speed: Most Wanted, con su
propio runtime y su propia emulación de la GPU de la Xbox 360:
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker), en el fork Android de
onenonlygit (SpeedBreaker One). No comparte nada en tiempo de ejecución con los otros dos
motores de este repositorio (el de Xenos de `android/` y el nativo de `nativo/`); vive aquí
para que todo lo de Android esté en un sitio. GPL-3.0, como el resto, con sus `COPYING`,
`NOTICE` y `LICENSES/`.

## Por qué está aquí

En un Vivo X300 Pro (Dimensity 9500, Mali-G1-Ultra MC12, driver r54p1, Android 17):

- El motor de Xenos **no puede**: el SDK exige `vertexPipelineStoresAndAtomics` y el driver de
  Mali no lo tiene (la sonda lo dice: `Emular la Xenos: falta vertexPipelineStoresAndAtomics`).
- El nativo **pasaría** por los rasgos del driver, pero hace falta el disco PAL España además del
  USA para compilarlo, y nunca se ha ejecutado en un Mali.
- SpeedBreaker **corre**: se compila solo con el disco USA, y sus shaders de vértices solo leen
  sus buffers, así que no necesita ese rasgo. Carreras a ~45–60 fps según el usuario; en el
  registro de la primera sesión buena, de 24,9 a 60 mezclando menús, cargas y carrera.

## De dónde sale

[PredatorWoah/speedbreaker-mali](https://github.com/PredatorWoah/speedbreaker-mali), rama
`android/rp6-bringup`, commit `3fd1250`, más las 8 de una sesión de Codex en ese Vivo,
aplicadas con `git am` desde su entrega. El árbol resultante es idéntico, fichero a fichero, al
de la entrega. Fuera se quedaron `builds/` (dos APK partidos en trozos: llevan el juego
traducido dentro) y `.github/` (sus workflows no se ejecutan desde una subcarpeta).

| Commit de Codex | Qué arregla |
|---|---|
| Support workspace-local Android toolchains on macOS | Compilar en un Mac de 8 GB |
| Expose Android capabilities and make probe results dismissible | La sonda dice qué tiene la GPU y se puede cerrar |
| Decode unsupported BC textures in the staging untile shader | Mali sin BC: se descomprimen en la GPU, al destejer. Comparado con un decodificador de referencia (~1,6 M texels) y en el móvil, sin diferencias |
| Arrange launcher actions side by side for landscape screens | El lanzador en apaisado |
| Add Android touch gamepad and sensor landscape orientation | Mando táctil |
| Avoid general shader dispatch loops for forward-only CF on Vivo Mali | **El cuelgue al cargar la partida guardada.** El compilador de shaders de Mali tardaba más de 20 s (minutos en la partida) con un shader traducido como bucle con `switch`; con su flujo de control solo hacia delante emitido en orden, 0,06 s. Bucles, llamadas y saltos atrás siguen por el camino general |
| Scope copied guest-memory compatibility to the observed Mali G1 driver | **Las rayas y bloques verdes de vídeos y menús**: con la memoria del juego importada sin copia, este driver se equivoca; con la copia sincronizada que SpeedBreaker ya tenía, no |
| Add editable touch layout and separate racing steering buttons | Editor del mando táctil y botones de girar separados |

Sus pruebas sintéticas, sin nada del juego, pasan también aquí (Clang 20, Linux): los 67 casos
de flujo de control, los de predicados y los de BC.

## Lo que se le ha añadido aquí

- **Los arreglos de Mali, por familia y no por una versión del driver.** Solo se activaban con
  el driver `226496512` exacto: una actualización del sistema los apagaba en silencio, y
  volvían las rayas y el cuelgue al cargar. Ahora, cualquier Mali-G1 (Ultra, Premium, Pro); con
  otro driver, una línea en el log lo dice. `NFSMW_FORWARD_CF=0` y
  `NFSMW_SHARED_MEMORY=import` siguen sirviendo para comparar sin ellos
  (`runtime/gpu/renderer.cpp`, `ObservedMaliG1Driver`).
- **Registro en Descargas** (`DownloadsLog.java`, adaptado de `RegistroDescargas`): cada
  partida o sonda deja en `Download/SpeedBreaker-logs/` el logcat de la app, el registro nativo
  de la sesión (`files/data/speedbreaker/logs/<hora>.log`, que es adonde va su stderr) copiado según crece, y
  una línea `diag` cada 5 s (estado y margen térmico, batería, RAM libre, RSS, Hz y MHz de cada
  núcleo). Al abrir el lanzador, si la partida anterior acabó mal, el motivo de Android, el
  logcat que quedaba y el tombstone. Puesto por defecto; el botón **Logs to Downloads** del
  lanzador lo apaga.
- **Pantalla a 60 Hz al jugar.** El juego va a 60 o 30 fps; en un panel a 90 Hz, como viene
  ese Vivo, ninguno divide el refresco y la imagen tiembla con los fps clavados. Se pide el modo
  de 60 Hz (o el múltiplo de 60 más bajo) y `Surface.setFrameRate(60)`.

## Compilar

Lo de su entrega, resumido; el detalle está en
[speedbreaker/android/README.md](../speedbreaker/android/README.md) y en sus scripts.
Probado por Codex en un Mac M1 de 8 GB; la sonda sin juego, también en Linux.

```sh
cd speedbreaker
bash scripts/android/setup_macos.sh            # en un Mac; en Linux, NDK 28.2 y CMake 3.31.6 a mano
source scripts/android/macos_env.sh
JOBS=2 bash scripts/android/prepare_recompiler.sh
JOBS=2 bash scripts/android/prepare_deps.sh
bash scripts/android/generate_ppc.sh /ruta/a/tu/default.xex     # el de TU disco USA
cd android
bash gradlew --no-daemon -Dorg.gradle.jvmargs=-Xmx1536m \
  -PsbDiagnostics=OFF -PsbBringup=ON -PsbCmakeVersion=3.31.6 \
  -PsbNativeJobs=2 :app:assembleRelease --max-workers=2
```

Solo acepta el `default.xex` de la USA con SHA-256
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`. **El APK que sale lleva el
juego: no se publica.** La sonda sin juego (`prepare_deps.sh --probe-only` y
`-PsbDiagnostics=ON -PsbBringup=ON`) no lleva nada del juego; es la que compila la CI.

Para actualizar una instalación sin borrarla hace falta la misma clave de firma
(`SB_KEYSTORE`, `SB_KEY_ALIAS`, `SB_STORE_PASSWORD`, `SB_KEY_PASSWORD`, de tu entorno, nunca
en el repositorio).

## Lo que queda

- **Coches demasiado brillantes.** Su muestreo de texturas no usa los campos de signo, gamma y
  exponente de la Xenos; que sea eso en los coches no está comprobado. No atenuar a ojo.
- **Trucos** (nitro infinito, todos los coches...): investigados, sin hacer.
- Sin comprobar todavía: una carrera completa y otra seguida, el modo carrera, volver de
  segundo plano, los dos lados en apaisado, y temperatura y memoria a lo largo de varias
  carreras.
- El ajuste a 60 Hz y el registro en Descargas de aquí no se han probado aún en el móvil.
