# Bobinadora CNC v10.5

![Bobinadora CNC](docs/images/bobinadora_CNC.jpg)

Proyecto de bobinadora CNC automatizada basado en **ESP32-S3**, LVGL 9.x y la
placa **JC4827W543** con pantalla RGB de 4,3 pulgadas y touch GT911.

La aplicación permite bobinar transformadores y bobinas de nido de abeja con
control de velocidad, avance del carro, capas y parámetros configurables.

## Novedades v10.5

- Actualización de firmware desde tarjeta MicroSD mediante `manifest.json`.
- Validación de placa, tamaño y hash SHA-256 antes de instalar.
- Instalación OTA con particiones `app0`/`app1` y reinicio manual posterior.
- Barra de progreso actualizada durante la validación y escritura.
- Pantallas de configuración creadas bajo demanda y destruidas al salir.
- Mejoras de estabilidad en homing, navegación y tarea de interfaz.
- Caché de compilación persistente de PlatformIO.

## Modos de bobinado

- **Transformador**: bobinado capa a capa con avance calculado según el hilo.
- **Nido de abeja**: bobinado cruzado con desfase angular configurable.
- **Control manual**: movimiento independiente de los ejes para preparar la
  posición inicial.

## Guía visual

### Selección y configuración

![Selección de modo](docs/images/seleccione_modo.jpg)

![Parámetros del transformador](docs/images/transformador_parametros.jpg)

![Parámetros del nido de abeja](docs/images/nido_abeja_parametros.jpg)

### Proceso de bobinado

![Bobinado de transformador](docs/images/transformador_bobinando.jpg)

![Bobinado de nido de abeja](docs/images/nido_abaja_bobinando.jpg)

### Otras pantallas

![Pantalla de inicio](docs/images/pantalla_inicio.jpg)

![Edición de datos](docs/images/editar_datos.jpg)

![Control manual](docs/images/control_manual.jpg)

## Actualización por MicroSD

La descarga recomendada para usuarios está en
[releases/](releases/). Para una instalación rápida, descarga el paquete
[Bobinadora_CNC_v10.5_SD.zip](releases/v10.5/Bobinadora_CNC_v10.5_SD.zip) y
extráelo directamente en la raíz de la tarjeta SD.

La tarjeta debe contener la versión con esta estructura:

```text
/firmware/10.5/manifest.json
/firmware/10.5/bobinadora.bin
```

En la máquina: **Ajustes → Actualizar firmware → Buscar en SD**. Selecciona la
versión, pulsa **Instalar seleccionada** y, cuando finalice, pulsa
**Reiniciar para aplicar**.

El manifiesto debe declarar `version`, `board`, `firmware`, `size` y `sha256`.
La placa compatible es `JC4827W543`. La actualización por WiFi queda preparada
para una fase posterior.

## Flujo de trabajo recomendado

1. Enciende la máquina y comprueba que la pantalla responde correctamente.
2. Realiza el homing para establecer el origen del carro.
3. Selecciona **Transformador** o **Nido de abeja**.
4. Introduce los parámetros del carrete, hilo, vueltas y velocidad.
5. Usa **Control manual** si necesitas ajustar la posición inicial.
6. Pulsa **Bobinar** y supervisa el proceso desde la pantalla de trabajo.

Las pantallas de configuración se crean al entrar y se destruyen al salir. Esto
reduce el consumo permanente de memoria y permite añadir nuevos submenús con
menos riesgo de saturar la interfaz.

## Solución rápida de problemas

- **No aparece una versión en la SD**: comprueba que la carpeta está dentro de
  `/firmware/` y que contiene `manifest.json` y `bobinadora.bin`.
- **Hash distinto**: vuelve a copiar el firmware y el manifiesto como pareja;
  el valor `sha256` debe corresponder exactamente al archivo `.bin`.
- **La instalación termina y pide reiniciar**: es el comportamiento esperado;
  pulsa **Reiniciar para aplicar** cuando la barra llegue al 100 %.
- **La SD no se reconoce**: expúlsala correctamente en el ordenador, prueba
  FAT32 y comprueba que está completamente insertada en el lector.
- **La placa se reinicia durante una actualización**: no retires la
  alimentación, espera al arranque y revisa el monitor serie a 115200 baudios.

## Hardware

| Componente | Configuración |
| --- | --- |
| Placa | JC4827W543 / ESP32-S3 |
| Pantalla | RGB LCD 480x272 / NV3041A |
| Touch | GT911, SDA 8, SCL 4, INT 3, RESET 38 |
| Eje X | STEP 5, DIR 9, ENABLE 14, LIMIT 46 |
| Eje Y | STEP 6, DIR 7, ENABLE 15, LIMIT 16 |
| MicroSD | SPI: SCK 12, MOSI 11, MISO 13, CS 10 |

## Desarrollo

El proyecto se compila con PlatformIO en VS Code:

```text
platformio run -e JC4827W543
platformio run -e JC4827W543 -t upload
```

Ramas principales:

- `master`: versión estable publicada.
- `develop`: futuras mejoras y pruebas.
- Etiquetas `v10.3`, `v10.4`, `v10.5`: versiones reproducibles.

Las notas detalladas de esta versión están en
[`docs/RELEASE_NOTES_V10_5.md`](docs/RELEASE_NOTES_V10_5.md).

## Publicar una nueva versión

1. Actualiza `FirmwareInfo::VERSION` y `FirmwareInfo::DISPLAY_NAME` en
   `include/config.h`.
2. Actualiza las notas de lanzamiento y genera el firmware.
3. Calcula el tamaño y el SHA-256 del archivo `.bin`.
4. Prepara `/firmware/<version>/manifest.json` y `bobinadora.bin`.
5. Prueba la instalación desde SD antes de crear la etiqueta de Git.
6. Publica el commit en `develop` y, cuando esté validado, avanza `master` y
   crea la etiqueta correspondiente, por ejemplo `v10.6`.

## Licencia

GPL v3. Consulta el archivo `LICENSE` para conocer los términos completos.
