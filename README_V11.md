# V11 Simulador - Diario tecnico

Este archivo acompana la rama experimental `v11-simulador`.
La intencion es dejar trazabilidad de las decisiones antes de mover codigo de la
version estable v10.3.

## Estado inicial

- Rama base: `v11-simulador`, creada desde `master`.
- `master` compila correctamente con PlatformIO antes de iniciar esta rama.
- El firmware v10.3 queda como referencia estable.
- Todavia no existe `core/`.
- Todavia no existe `simulator/`.

## Que es `core/`

`core/` sera la carpeta para la logica pura de bobinado.

Debe poder compilar en PC sin depender de:

- Arduino
- ESP32
- LVGL
- FreeRTOS
- `Serial`
- pines fisicos
- `digitalWrite`, `micros`, `delayMicroseconds`

En la primera fase, `core/` deberia contener solo la logica minima del modo
transformador:

- parametros de entrada del bobinado
- calculos derivados
- estado logico del bobinado
- simulacion de pasos del eje Y
- eventos logicos del eje X, vueltas, capas y fin de bobinado

La generacion real de pulsos STEP/DIR seguira perteneciendo al firmware ESP32.

## Como ejecutar el simulador

El simulador todavia no esta implementado.

Cuando exista, debe incluir un comando simple para probarlo desde Windows:

```powershell
.\tools\run-simulator.bat
```

Ese script debe encargarse de compilar y ejecutar el simulador minimo, para no
depender de recordar todos los comandos manuales.

La ruta manual esperada sera algo parecido a:

```powershell
cd simulator
cmake -S . -B build
cmake --build build
.\build\transformer_sim.exe
```

Si se decide una opcion mas simple para la Fase 1, podria ejecutarse con un
compilador C++ directamente. Este README se actualizara cuando el comando real
quede definido.

## Que no se ha tocado

En este punto no se ha modificado la logica funcional de v10.3.

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

La Fase 1 debe ser aditiva: crear `core/` y `simulator/` sin conectar todavia el
firmware ESP32 a la nueva arquitectura.

El objetivo inmediato no es cambiar como bobina la maquina, sino poder estudiar
y probar la logica del modo transformador en PC.

La Fase 1 debe incluir tambien `tools/run-simulator.bat` como entrada sencilla
para compilar y ejecutar el simulador.
