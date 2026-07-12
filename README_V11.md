# V11 Core - Diario tecnico

Este archivo acompana la rama experimental `v11-simulador`.
La intencion es dejar trazabilidad de las decisiones antes de mover codigo de la
version estable v10.3.

Nota de direccion: el simulador en PC queda como opcion secundaria porque este
equipo no tiene espacio para instalar un toolchain C++ de escritorio. A partir
de ahora la prioridad es que el firmware PlatformIO siga compilando y que
`core/` pueda integrarse poco a poco en ESP32. La validacion actual se hara con
PlatformIO y pruebas en placa.

## Estado inicial

- Rama base: `v11-simulador`, creada desde `master`.
- `master` compila correctamente con PlatformIO antes de iniciar esta rama.
- El firmware v10.3 queda como referencia estable.
- `core/` contiene la primera logica pura del modo transformador.
- `simulator/` contiene un ejecutable de consola opcional para PC. Se conserva
  como trabajo futuro, pero no es necesario para continuar.

## Que es `core/`

`core/` es la carpeta para la logica pura de bobinado.

Debe mantenerse independiente de:

- Arduino
- LVGL
- FreeRTOS
- `Serial`
- pines fisicos
- `digitalWrite`, `micros`, `delayMicroseconds`

La primera version de `core/` contiene solo la logica minima del modo
transformador:

- parametros de entrada del bobinado
- calculos derivados
- estado logico del bobinado
- simulacion de pasos del eje Y
- eventos logicos del eje X, vueltas, capas y fin de bobinado

La generacion real de pulsos STEP/DIR seguira perteneciendo al firmware ESP32.

## Como ejecutar el simulador

El simulador de PC es opcional y queda como futuro. No se instalara CMake, MSVC,
MinGW, clang ni ningun toolchain pesado solo para ejecutarlo en este equipo.

Comando simple desde Windows:

```powershell
.\tools\run-simulator.bat
```

Ese script compila y ejecuta el simulador minimo. Requiere que `cmake` este
disponible en `PATH`.

CMake es la opcion principal porque mantiene separada la configuracion del
compilador concreto. Si no hay CMake disponible, hay dos alternativas directas:

```powershell
.\tools\run-simulator-gpp.bat
.\tools\run-simulator-msvc.bat
```

`run-simulator-gpp.bat` requiere `g++` en `PATH`. `run-simulator-msvc.bat`
requiere ejecutar desde una Developer Command Prompt de Visual Studio, donde
`cl.exe` este disponible.

Ruta manual equivalente:

```powershell
cd simulator
cmake -S . -B build
cmake --build build
.\build\transformer_sim.exe
```

En generadores multi-configuracion, como Visual Studio, el ejecutable puede
quedar en:

```powershell
.\build\Release\transformer_sim.exe
```

La ruta de validacion actual pasa por PlatformIO y por pruebas en placa: primero
mantener el firmware compilando y despues integrar `core/` gradualmente en el
target ESP32.

## Que no se ha tocado

En esta fase no se ha modificado la logica funcional de v10.3.

Archivos especialmente protegidos en la primera fase:

- `lib/PINS_JC4827W543.h`
- `src/ui_screens.cpp`
- `src/ui_handlers.cpp`
- `src/ui_components.cpp`
- `include/ui_screens.h`
- `include/ui_handlers.h`
- `include/ui_components.h`
- `include/config.h`
- `include/motor_task_optimized.h`
- `include/persistence.h`
- `src/profile_manager.cpp`
- `src/main.cpp`
- `platformio.ini`

## Criterio de trabajo

La Fase 1 es aditiva: crea `core/` y `simulator/` sin conectar todavia el
firmware ESP32 a la nueva arquitectura.

El objetivo inmediato no es cambiar como bobina la maquina, sino aislar la
logica pura para poder reutilizarla desde el firmware.

La Fase 1 incluye tambien `tools/run-simulator.bat` como entrada sencilla para
compilar y ejecutar el simulador.

## Prioridad a partir de ahora

- Mantener `master` como firmware estable.
- Mantener `v11-simulador` como rama experimental.
- No eliminar `core/`.
- No borrar `simulator/`; queda como opcion futura.
- No instalar toolchains ni dependencias pesadas para simulacion PC.
- Tratar `simulator/` y `tools/run-simulator*.bat` como ayudas opcionales.
- Priorizar que `pio run` compile el firmware y que los cambios se puedan probar
  en placa.
- Integrar `core/` en ESP32 por pasos pequenos, sin tocar UI ni pines hasta que
  sea necesario y aprobado.
