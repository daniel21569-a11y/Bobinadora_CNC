# Arquitectura UI v10.4

Este documento describe el estado actual de la arquitectura LVGL de v10.4 y
debe leerse antes de tocar la UI.

## Estado actual

La UI se construye en `src/ui_screens.cpp`. Las acciones y cambios de pantalla
se concentran en `src/ui_handlers.cpp`. Los helpers visuales comunes estan en
`src/ui_components.cpp`.

El firmware crea las pantallas principales durante `setup()`, antes de cargar la
pantalla principal y antes de marcar `lvgl_ready = true`.

Los submenus de `Ajustes` no siguen ese ciclo de vida: son temporales, se crean
bajo demanda al pulsar su boton y se destruyen al volver a `Ajustes`.

## Pantallas LVGL reales

Las pantallas reales actuales son objetos creados con `lv_obj_create(NULL)`:

- `screen_main`: pantalla principal.
- `screen_modo_selection`: seleccion de modo.
- `screen_config`: configuracion de Transformador.
- `screen_config_honeycomb`: configuracion de Nido de Abeja.
- `screen_winding`: control de bobinado.
- `screen_manual_control`: control manual.
- `screen_settings`: ajustes.

Estas pantallas principales son persistentes y se crean desde
`UIScreens::init_all_screens()`.

Los submenus de `Ajustes` son pantallas temporales:

- `screen_settings_brightness`
- `screen_settings_info`
- `screen_settings_machine_status`

Sus punteros se inicializan a `nullptr`, se crean solo al pulsar su boton y se
destruyen al volver a `Ajustes`.

## Orden de creacion

`UIScreens::init_all_screens()` crea las pantallas en este orden:

1. `crear_pantalla_principal()`
2. `crear_pantalla_seleccion_modo()`
3. `crear_pantalla_configuracion()`
4. `crear_pantalla_configuracion_honeycomb()`
5. `crear_pantalla_bobinado()`
6. `crear_pantalla_control_manual()`
7. `crear_pantalla_ajustes()`

Al final de `setup()`, despues de crear las pantallas, se carga:

```cpp
lv_scr_load(UIScreens::screen_main);
```

## Navegacion

`UIHandlers::btn_navegacion_handler()` lee `user_data` del objeto pulsado y
ejecuta `lv_scr_load_anim()` contra la pantalla correspondiente.

Cadenas `user_data` usadas para navegacion:

- `SELECCION_MODO` -> `screen_modo_selection`
- `CONFIG` -> `screen_config`
- `CONFIG_HC` -> `screen_config_honeycomb`
- `BOBINADO` -> `screen_winding`
- `MANUAL` -> `screen_manual_control`
- `AJUSTES` -> `screen_settings`
- `PRINCIPAL` -> `screen_main`

Los submenus de `Ajustes` no deben crearse en `init_all_screens()`. Su puntero
debe inicializarse a `nullptr`, la pantalla se debe crear solo al pulsar el
boton correspondiente y se debe destruir al volver a `Ajustes`.

## Header comun

`UI::create_header()` crea un header con titulo, boton atras opcional y boton de
accion opcional.

Limitacion importante: si se pasa un callback de vuelta, el boton atras se crea
siempre con:

```cpp
lv_obj_set_user_data(btn_back, (void *)"PRINCIPAL");
```

Por tanto, el header comun vuelve siempre a `PRINCIPAL`. No es adecuado para
subnavegacion interna de `Ajustes` que deba volver a `AJUSTES`.

## Objetos LVGL globales principales

Pantallas globales:

- `screen_main`
- `screen_modo_selection`
- `screen_config`
- `screen_config_honeycomb`
- `screen_winding`
- `screen_manual_control`
- `screen_settings`

Campos de configuracion globales:

- `ta_diametro_alambre`
- `ta_longitud_bobinado`
- `ta_vueltas_total`
- `ta_velocidad_rpm`
- `sw_detener_en_capas`
- `ta_hc_diametro_hilo`
- `ta_hc_diametro_carrete`
- `ta_hc_ancho_carrete`
- `ta_hc_desfase_grados`
- `ta_hc_num_vueltas`
- `ta_hc_velocidad`

Labels y barra de bobinado:

- `label_estado`
- `label_modo_actual`
- `label_vueltas_actuales`
- `label_vueltas_totales`
- `label_progreso`
- `bar_progreso`
- `label_capa_info`
- `label_velocidad_actual`
- `label_tiempo_restante`

Los labels de informacion de `Ajustes` son `static` dentro de
`src/ui_screens.cpp`.

## Refresco de labels

`screen_winding_load_handler()` refresca al entrar en `screen_winding`:

- vueltas totales
- modo actual

`update_winding_screen()` corre desde un timer cada 100 ms, pero primero valida:

```cpp
if (lv_scr_act() != UIScreens::screen_winding)
  return;
```

Actualiza estado, vueltas, progreso, RPM, capa y tiempo restante.

`refresh_system_info_event_cb()` actualiza los labels de `Ajustes` al cargar la
pantalla y despues de pulsar Bajo, Medio o Alto.

## Riesgos detectados

- Crear submenus nuevos dentro de `init_all_screens()` aumenta el trabajo y la
  memoria usada durante el arranque.
- Dejar varios submenus de `Ajustes` vivos en memoria puede bloquear o reiniciar
  la placa.
- La presion de memoria y de objetos LVGL puede provocar regresiones fisicas
  aunque la build compile.
- Refrescar objetos no creados o no inicializados puede romper la UI; comprobar
  punteros antes de actualizar.
- Los callbacks encadenados en un mismo boton hacen mas dificil razonar sobre
  orden y efectos.
- `UI::create_header()` vuelve siempre a `PRINCIPAL`, asi que no sirve tal cual
  para volver a una vista interna de `Ajustes`.

## Leccion aprendida

Una prueba fisica posterior valido que el hardware y LVGL si pueden abrir
pantallas adicionales cuando se crean bajo demanda. La regresion no demuestra
que toda pantalla adicional sea insegura; el patron peligroso es ampliar de
golpe la navegacion o crear submenus durante el arranque sin prueba fisica
intermedia.

## Patron recomendado para futuros ajustes

- Pantallas principales:
  - son persistentes
  - se crean en `UIScreens::init_all_screens()`
  - no se destruyen al navegar
- Submenus de `Ajustes`:
  - declarar el puntero global de pantalla e inicializarlo a `nullptr`
  - no llamar a su funcion de creacion desde `init_all_screens()`
  - crear la pantalla solo cuando el usuario pulse el boton del submenu
  - al pulsar `Volver`, cargar primero `screen_settings`
  - destruir despues la pantalla del submenu cerrado con `lv_obj_delete_async()`
  - poner su puntero global a `nullptr`
- No dejar varios submenus de `Ajustes` vivos en memoria.
- Cada submenu debe tener un boton volver local hacia `AJUSTES` que aplique este
  ciclo de destruccion.
- No usar `UI::create_header()` para submenus de `Ajustes` si se necesita volver
  a `AJUSTES`, porque el header comun vuelve a `PRINCIPAL`.
- Anadir submenus uno a uno y probar fisicamente cada fase antes de continuar.
- Evitar timers y refrescos dinamicos en el primer paso de cada submenu.

## Submenus aplicados

- `Retroiluminacion`: primer submenu real de `Ajustes` creado con lazy creation.
  No se crea en `init_all_screens()`; se crea al pulsar `Abrir
  retroiluminacion`, vuelve localmente a `AJUSTES` y se destruye al volver.
- `Informacion del sistema`: submenu temporal de `Ajustes`; se crea bajo
  demanda y se destruye al volver.
- `Estado de la maquina`: submenu temporal de `Ajustes`; se crea bajo demanda y
  se destruye al volver.

## Checklist obligatorio antes de tocar UI

- Leer este documento.
- Ejecutar build con `-j 1`.
- Hacer prueba fisica de arranque.
- Probar tactil.
- Probar navegacion afectada.
- Esperar 2-3 minutos sin reinicio.
