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

## Fase 2 minima

`ConfigTransformador::calcular_parametros()` mantiene su API publica, pero ahora
delega el calculo de parametros derivados en `winding::calculate_transformer()`.

Esta integracion solo reutiliza el calculo puro del core:

- no conecta `motor_task_optimized.h` al core
- no cambia la generacion de pulsos STEP/DIR
- no cambia pines
- no toca la UI
- no convierte `TransformerEngine` a header-only

Implementacion:

- solo `winding::calculate_transformer()` queda inline/header-only para que el
  firmware PlatformIO pueda usarlo sin cambiar `platformio.ini`
- `TransformerEngine` sigue implementado en `core/src/transformer_core.cpp`
- `ConfigTransformador` conserva sus campos publicos actuales y copia los
  resultados derivados del core a esos mismos campos

El objetivo es comprobar que el firmware puede empezar a consumir `core/` sin
cambiar comportamiento visible.

## Fase 3 minima

`ConfigNidoAbeja::calcular_parametros()` mantiene su API publica, pero ahora
delega el calculo de parametros derivados en `winding::calculate_honeycomb()`.

Esta integracion solo reutiliza el calculo puro del core:

- no conecta la logica honeycomb de `motor_task_optimized.h` al core
- no cambia la generacion de pulsos STEP/DIR
- no cambia pines
- no toca la UI
- no cambia `src/main.cpp`
- no cambia `platformio.ini`

Detalle de equivalencia:

| Campo actual | Origen en core |
| --- | --- |
| `desfase_grados` | `HoneycombDerived::desfase_grados` |
| `factor_desfase` | `HoneycombDerived::factor_desfase` |
| `vueltas_por_capa` | `HoneycombDerived::vueltas_por_capa` |
| `capas_estimadas` | `HoneycombDerived::capas_estimadas` |
| `grosor_estimado` | `HoneycombDerived::grosor_estimado` |
| `step_ratio_X_per_Y` | `HoneycombDerived::step_ratio_x_per_y` |
| `pasos_X_recorrido_completo` | `HoneycombDerived::pasos_x_recorrido_completo` |
| `pasos_Y_por_ciclo` | `HoneycombDerived::pasos_y_por_ciclo` |

Se conserva el comportamiento anterior en el caso especial donde
`factor_desfase < 0.1`: el calculo ajusta `factor_desfase` a `0.1` y actualiza
`desfase_grados` a `(factor_desfase - 1.0) * 360.0`.

### Equivalencia numerica revisada

Se comparo el calculo anterior de `ConfigTransformador::calcular_parametros()`
con `winding::calculate_transformer()` usando las mismas constantes mecanicas:

- `PASOS_POR_MM_X = 320.0`
- `PASOS_POR_VUELTA_Y = 3200`

| Caso | Diametro | Longitud | Vueltas | Vueltas/capa | Capas | Grosor | Ratio X/Y | Limite X |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Base | 0.5 | 50 | 1000 | 100 | 10 | 5.0 | 0.05 | 16000 |
| Hilo fino | 0.2 | 10 | 500 | 50 | 10 | 2.0 | 0.02 | 3200 |
| Diametro cero | 0.0 | 50 | 1000 | 0 | 0 | 0.0 | 0.0 | 16000 |
| Longitud no divisible | 0.3 | 10 | 100 | 33 | 4 | 1.2 | 0.03 | 3200 |

Conclusiones:

- `floor(longitud / diametro)` mantiene el mismo resultado para
  `vueltas_por_capa`.
- `ceil(vueltas_total / vueltas_por_capa)` mantiene el mismo resultado para
  `capas_estimadas`.
- `roundf(longitud * pasos_por_mm)` y `std::round(...)` mantienen el mismo
  resultado en los casos revisados.
- Los casts a enteros mantienen los mismos valores observados.
- `calcular_parametros()` sigue sin validar entradas; la validacion permanece
  separada en `ConfigTransformador::validar()`.

## Tipos preparatorios de estado

Se anade `core/include/winding/machine_state.h` como preparacion para una futura
separacion entre UI, estado logico y motor.

Estos tipos son puros y no estan conectados todavia al firmware:

- `MachineStatus`
- `WindingMode`
- `MachineCommand`
- `MachineError`
- `MachineSnapshot`

`MachineSnapshot` refleja los campos logicos principales que hoy viven en
`EstadoSistema`, pero sin `volatile`, sin metodos con efectos, sin `Serial` y
sin dependencias de Arduino, LVGL, FreeRTOS, pines o drivers.

Esta fase no cambia comportamiento visible. El firmware sigue usando
`EstadoSistema` y `motor_task_optimized.h` como antes.

## Adaptadores firmware -> core

Se anade `include/winding_core_adapters.h` como frontera preparatoria entre los
tipos actuales del firmware y los tipos puros de `machine_state.h`.

El adaptador es header-only y no esta conectado todavia al firmware. Solo define
conversiones de firmware hacia core:

- `EstadoBobinado` -> `MachineStatus`
- `ModoBobinado` -> `WindingMode`
- `TipoMovimiento` -> `MachineCommand`
- `EstadoSistema` -> `MachineSnapshot`

No existen conversiones core -> firmware en esta fase.

Notas de mapeo:

- `MachineStatus::Complete` no tiene equivalente desde el firmware actual.
- `EstadoBobinado::ERROR` se refleja como `MachineSnapshot.error =
  MachineError::Unknown`.
- `TipoMovimiento` solo cubre movimiento manual y homing; no representa
  comandos de alto nivel como Start, Pause, Resume o Stop.

Esta fase no cambia comportamiento visible porque ningun archivo del firmware
incluye todavia el adaptador.

## Reglas de verificacion

- En esta maquina, usar PlatformIO en modo secuencial para evitar builds
  paralelas colgadas:

```powershell
& 'C:\Users\Daniel\.platformio\penv\Scripts\python.exe' -m platformio run -j 1
```

- No tocar `include/motor_task_optimized.h` sin plan previo, diff revisado y
  una estrategia clara para comparar comportamiento.
- No conectar `include/winding_core_adapters.h` al flujo real del firmware sin
  pruebas previas.
- Mantener protegidos UI y pines:
  - `src/ui_*`
  - `lib/PINS_JC4827W543.h`
  - `platformio.ini`
  - `src/main.cpp`

## Checkpoint de validacion en placa

Validado en placa:

- arranque OK
- UI OK
- touch OK
- navegacion basica OK
