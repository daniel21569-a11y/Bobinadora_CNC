# Descargas de firmware para tarjeta SD

Esta carpeta contiene las versiones de firmware preparadas para actualizar la
bobinadora mediante tarjeta MicroSD.

## Última versión

La versión estable actual es **v10.5**.

- [Paquete SD v10.5 listo para copiar](v10.5/Bobinadora_CNC_v10.5_SD.zip)
- [Archivos de v10.5](v10.5/)
- [Notas de la versión](../docs/RELEASE_NOTES_V10_5.md)

## Instalación rápida

1. Descarga `Bobinadora_CNC_v10.5_SD.zip`.
2. Extrae su contenido directamente en la raíz de la tarjeta SD.
3. Comprueba que la tarjeta queda así:

```text
/firmware/10.5/manifest.json
/firmware/10.5/bobinadora.bin
```

4. Inserta la tarjeta en la bobinadora.
5. Abre **Ajustes → Actualizar firmware → Buscar en SD**.
6. Selecciona la versión, pulsa **Instalar seleccionada** y espera al 100 %.
7. Pulsa **Reiniciar para aplicar**.

No renombres el archivo `.bin` ni edites el `manifest.json`: el manifiesto
contiene el tamaño y la firma SHA-256 necesarios para validar la actualización.

## Versiones antiguas

Las versiones anteriores se conservan dentro de sus carpetas correspondientes.
Para actualizar, utiliza siempre la carpeta con el número de versión más alto
compatible con tu placa.
