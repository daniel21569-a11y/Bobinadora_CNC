# Bobinadora CNC v10.5

## Resumen

La v10.5 consolida la arquitectura de interfaz y añade una primera actualización
de firmware desde tarjeta MicroSD para la placa JC4827W543.

## Mejoras incluidas

- Versionado centralizado en `FirmwareInfo`, utilizado por la pantalla de
  información y por los mensajes de arranque.
- Nueva pantalla `Actualizar firmware` dentro de Ajustes.
- Búsqueda de varias versiones en `/firmware/<version>/` mediante
  `manifest.json`.
- Comprobación de placa, tamaño y hash SHA-256 antes de escribir.
- Escritura OTA en la partición alternativa y reinicio manual posterior.
- Barra de progreso actualizada durante la validación y la instalación.
- Pantallas de configuración de Transformador y Nido de Abeja creadas bajo
  demanda y destruidas al salir.
- Inicialización de la tarjeta SD durante el arranque para compartirla con el
  gestor de perfiles y el actualizador.
- Particiones OTA `app0`/`app1` y caché de compilación persistente de PlatformIO.
- Correcciones de estabilidad en homing, navegación y tarea de interfaz.

## Formato de actualización SD

```text
/firmware/10.5/manifest.json
/firmware/10.5/bobinadora.bin
```

El manifiesto debe declarar `version`, `board`, `firmware`, `size` y `sha256`.
La placa compatible para esta versión es `JC4827W543`.

## Notas de validación

- La actualización se probó con la placa conectada por USB y mediante tarjeta
  SD.
- Tras completar la escritura, el usuario debe pulsar `Reiniciar para aplicar`.
- La actualización por WiFi queda reservada para una versión futura.
