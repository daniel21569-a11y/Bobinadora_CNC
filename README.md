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

## Actualización por MicroSD

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

## Licencia

GPL v3. Consulta el archivo `LICENSE` para conocer los términos completos.
