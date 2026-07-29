#include "ui_screens.h"
#include "firmware_update.h"
#include "persistence.h"
#include "profile_manager.h"
#include "ui_handlers.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

void display_backlight(uint8_t brightness);

namespace UIScreens {
// Definición de pantallas
lv_obj_t *screen_main = nullptr;
lv_obj_t *screen_modo_selection = nullptr;
lv_obj_t *screen_config = nullptr;
lv_obj_t *screen_config_honeycomb = nullptr;
lv_obj_t *screen_winding = nullptr;
lv_obj_t *screen_manual_control = nullptr;
lv_obj_t *screen_settings = nullptr;
lv_obj_t *screen_settings_brightness = nullptr;
lv_obj_t *screen_settings_info = nullptr;
lv_obj_t *screen_settings_machine_status = nullptr;
lv_obj_t *screen_settings_firmware = nullptr;

// Campos de configuración TRANSFORMADOR
lv_obj_t *ta_diametro_alambre = nullptr;
lv_obj_t *ta_longitud_bobinado = nullptr;
lv_obj_t *ta_vueltas_total = nullptr;
lv_obj_t *ta_velocidad_rpm = nullptr;
lv_obj_t *sw_detener_en_capas = nullptr;

// Campos de configuración NIDO DE ABEJA
lv_obj_t *ta_hc_diametro_hilo = nullptr;
lv_obj_t *ta_hc_diametro_carrete = nullptr;
lv_obj_t *ta_hc_ancho_carrete = nullptr;
lv_obj_t *ta_hc_desfase_grados = nullptr;
lv_obj_t *ta_hc_num_vueltas = nullptr;
lv_obj_t *ta_hc_velocidad = nullptr;

// Labels de pantalla de bobinado
lv_obj_t *label_estado = nullptr;
lv_obj_t *label_modo_actual = nullptr;
lv_obj_t *label_vueltas_actuales = nullptr;
lv_obj_t *label_vueltas_totales = nullptr;
lv_obj_t *label_progreso = nullptr;
lv_obj_t *bar_progreso = nullptr;
lv_obj_t *label_capa_info = nullptr;
lv_obj_t *label_velocidad_actual = nullptr;
lv_obj_t *label_tiempo_restante = nullptr;

static lv_obj_t *label_info_display_name = nullptr;
static lv_obj_t *label_info_codename = nullptr;
static lv_obj_t *label_info_version = nullptr;
static lv_obj_t *label_info_brightness = nullptr;
static lv_obj_t *label_info_mode = nullptr;
static lv_obj_t *label_info_status = nullptr;
static lv_obj_t *label_machine_status_summary = nullptr;
static lv_obj_t *label_brightness_submenu_value = nullptr;
static lv_obj_t *slider_brightness_submenu = nullptr;
static lv_obj_t *firmware_status_label = nullptr;
static lv_obj_t *firmware_version_dropdown = nullptr;
static lv_obj_t *firmware_progress_bar = nullptr;
static lv_obj_t *firmware_install_button = nullptr;
static lv_obj_t *firmware_restart_button = nullptr;
static lv_timer_t *firmware_update_timer = nullptr;
static std::vector<FirmwareUpdate::Package> firmware_packages;
static volatile bool firmware_install_running = false;
static volatile bool firmware_install_finished = false;
static volatile bool firmware_install_success = false;
static volatile uint8_t firmware_install_progress = 0;
static String firmware_install_error;

static const char *mode_to_text(ModoBobinado mode) {
  return mode == ModoBobinado::TRANSFORMADOR ? "Transformador"
                                             : "Nido de Abeja";
}

static const char *status_to_text(EstadoBobinado status) {
  switch (status) {
  case EstadoBobinado::LISTO:
    return "LISTO";
  case EstadoBobinado::BOBINANDO:
    return "BOBINANDO";
  case EstadoBobinado::PAUSADO:
    return "PAUSADO";
  case EstadoBobinado::ERROR:
    return "ERROR";
  case EstadoBobinado::HOMING:
    return "HOMING";
  case EstadoBobinado::MANUAL:
    return "MANUAL";
  }
  return "DESCONOCIDO";
}

static void update_system_info_labels() {
  if (label_info_display_name) {
    lv_label_set_text(label_info_display_name, FirmwareInfo::DISPLAY_NAME);
  }
  if (label_info_codename) {
    lv_label_set_text(label_info_codename, FirmwareInfo::CODENAME);
  }
  if (label_info_version) {
    lv_label_set_text(label_info_version, FirmwareInfo::VERSION);
  }
  if (label_info_brightness) {
    lv_label_set_text_fmt(label_info_brightness, "%u/255",
                          Sistema::estado.brillo_backlight);
  }
  if (label_info_mode) {
    lv_label_set_text(label_info_mode, mode_to_text(Sistema::estado.modo));
  }
  if (label_info_status) {
    lv_label_set_text(label_info_status, status_to_text(Sistema::estado.estado));
  }
  if (label_machine_status_summary) {
    lv_label_set_text_fmt(
        label_machine_status_summary,
        "Estado: %s | Modo: %s\nVueltas: %lu | Capa: %lu | Capas: %lu\nRPM: "
        "%.1f / %.1f\nCompletado: %s",
        status_to_text(Sistema::estado.estado),
        mode_to_text(Sistema::estado.modo), Sistema::estado.vueltas_completadas,
        Sistema::estado.vueltas_capa_actual, Sistema::estado.capas_completadas,
        Sistema::estado.rpm_actual, Sistema::estado.rpm_objetivo,
        Sistema::estado.bobinado_completado ? "Si" : "No");
  }
}

static void refresh_system_info_event_cb(lv_event_t *e) {
  update_system_info_labels();
}

static void update_brightness_submenu_label() {
  if (label_brightness_submenu_value) {
    lv_label_set_text_fmt(label_brightness_submenu_value, "Brillo: %u",
                          Sistema::estado.brillo_backlight);
  }
}

static void sync_brightness_submenu_controls() {
  if (slider_brightness_submenu) {
    lv_slider_set_value(slider_brightness_submenu,
                        Sistema::estado.brillo_backlight, LV_ANIM_OFF);
  }
  update_brightness_submenu_label();
}

static void brightness_submenu_screen_event_cb(lv_event_t *e) {
  sync_brightness_submenu_controls();
}

static void brightness_submenu_slider_changed_cb(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
  uint8_t brightness = (uint8_t)lv_slider_get_value(slider);
  Sistema::estado.brillo_backlight = brightness;
  display_backlight(brightness);
  update_brightness_submenu_label();
}

static void brightness_submenu_slider_released_cb(lv_event_t *e) {
  Persistence.saveBrightness(Sistema::estado.brillo_backlight);
}

static void settings_info_screen_event_cb(lv_event_t *e) {
  update_system_info_labels();
}

static void settings_machine_status_screen_event_cb(lv_event_t *e) {
  update_system_info_labels();
}

static void firmware_progress_cb(uint8_t progress) {
  firmware_install_progress = progress;
}

static void firmware_install_task(void *parameter) {
  FirmwareUpdate::Package *package =
      static_cast<FirmwareUpdate::Package *>(parameter);
  String error;
  bool success = FirmwareUpdate::install_from_sd(*package, error,
                                                 firmware_progress_cb);
  firmware_install_error = error;
  firmware_install_success = success;
  firmware_install_finished = true;
  firmware_install_running = false;
  delete package;
  vTaskDelete(nullptr);
}

static void firmware_update_timer_cb(lv_timer_t *timer) {
  if (firmware_progress_bar)
    lv_bar_set_value(firmware_progress_bar, firmware_install_progress,
                     LV_ANIM_OFF);

  if (firmware_install_running) {
    if (firmware_status_label)
      lv_label_set_text_fmt(firmware_status_label,
                            "Procesando actualizacion: %u%%",
                            firmware_install_progress);
    return;
  }

  if (!firmware_install_finished)
    return;

  firmware_install_finished = false;
  if (firmware_install_success) {
    if (firmware_status_label)
      lv_label_set_text(firmware_status_label,
                        "Actualizacion completada correctamente.\nPulse Reiniciar para aplicar.");
    if (firmware_restart_button)
      lv_obj_clear_state(firmware_restart_button, LV_STATE_DISABLED);
  } else {
    if (firmware_status_label)
      lv_label_set_text(firmware_status_label, firmware_install_error.c_str());
    if (firmware_install_button)
      lv_obj_clear_state(firmware_install_button, LV_STATE_DISABLED);
  }
}

static void refresh_firmware_packages() {
  if (!firmware_version_dropdown || !firmware_status_label)
    return;

  String error;
  if (!FirmwareUpdate::scan_sd(firmware_packages, error)) {
    lv_dropdown_set_options(firmware_version_dropdown, "Sin versiones");
    lv_dropdown_set_selected(firmware_version_dropdown, 0);
    lv_label_set_text(firmware_status_label, error.c_str());
    if (firmware_install_button)
      lv_obj_add_state(firmware_install_button, LV_STATE_DISABLED);
    return;
  }

  String options;
  for (size_t i = 0; i < firmware_packages.size(); ++i) {
    if (i > 0)
      options += "\n";
    options += firmware_packages[i].version;
    if (!firmware_packages[i].compatible)
      options += " (incompatible)";
  }
  lv_dropdown_set_options(firmware_version_dropdown, options.c_str());
  lv_dropdown_set_selected(firmware_version_dropdown, 0);
  lv_label_set_text_fmt(firmware_status_label, "%u versiones encontradas",
                        (unsigned)firmware_packages.size());
  if (firmware_install_button)
    lv_obj_clear_state(firmware_install_button, LV_STATE_DISABLED);
}

static void firmware_scan_button_cb(lv_event_t *e) {
  refresh_firmware_packages();
}

static void firmware_restart_button_cb(lv_event_t *e) {
  if (firmware_status_label)
    lv_label_set_text(firmware_status_label, "Reiniciando para aplicar...");
  delay(200);
  ESP.restart();
}

static void firmware_install_button_cb(lv_event_t *e) {
  if (firmware_install_running || !firmware_version_dropdown ||
      firmware_packages.empty())
    return;

  uint16_t selected = lv_dropdown_get_selected(firmware_version_dropdown);
  if (selected >= firmware_packages.size())
    return;

  lv_obj_add_state(firmware_install_button, LV_STATE_DISABLED);
  if (firmware_restart_button)
    lv_obj_add_state(firmware_restart_button, LV_STATE_DISABLED);
  firmware_install_progress = 0;
  firmware_install_finished = false;
  firmware_install_success = false;
  firmware_install_error = "";
  lv_label_set_text(firmware_status_label,
                    "Instalando. No retire la alimentacion...");
  FirmwareUpdate::Package *package =
      new FirmwareUpdate::Package(firmware_packages[selected]);
  firmware_install_running = true;
  TaskHandle_t task_handle = nullptr;
  if (xTaskCreatePinnedToCore(firmware_install_task, "FirmwareInstall", 16384,
                              package, 1, &task_handle, 0) != pdPASS) {
    delete package;
    firmware_install_running = false;
    lv_label_set_text(firmware_status_label,
                      "No se pudo iniciar la tarea de actualizacion");
    lv_obj_clear_state(firmware_install_button, LV_STATE_DISABLED);
    return;
  }
}

static void settings_submenu_back_handler(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  const char *id = (const char *)lv_obj_get_user_data(target);
  lv_obj_t *screen_to_delete = nullptr;

  if (strcmp(id, "CERRAR_AJUSTES_BRILLO") == 0) {
    screen_to_delete = screen_settings_brightness;
    screen_settings_brightness = nullptr;
    label_brightness_submenu_value = nullptr;
    slider_brightness_submenu = nullptr;
  } else if (strcmp(id, "CERRAR_AJUSTES_INFO") == 0) {
    screen_to_delete = screen_settings_info;
    screen_settings_info = nullptr;
    label_info_display_name = nullptr;
    label_info_codename = nullptr;
    label_info_version = nullptr;
    label_info_brightness = nullptr;
    label_info_mode = nullptr;
    label_info_status = nullptr;
  } else if (strcmp(id, "CERRAR_AJUSTES_ESTADO") == 0) {
    screen_to_delete = screen_settings_machine_status;
    screen_settings_machine_status = nullptr;
    label_machine_status_summary = nullptr;
  } else if (strcmp(id, "CERRAR_AJUSTES_FIRMWARE") == 0) {
    screen_to_delete = screen_settings_firmware;
    screen_settings_firmware = nullptr;
    firmware_status_label = nullptr;
    firmware_version_dropdown = nullptr;
    firmware_progress_bar = nullptr;
    firmware_install_button = nullptr;
    firmware_restart_button = nullptr;
    if (firmware_update_timer) {
      lv_timer_del(firmware_update_timer);
      firmware_update_timer = nullptr;
    }
    firmware_packages.clear();
  }

  lv_scr_load(screen_settings);
  if (screen_to_delete) {
    lv_obj_delete_async(screen_to_delete);
  }
}

void crear_pantalla_principal() {
  screen_main = lv_obj_create(NULL);
  lv_obj_add_style(screen_main, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_main, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_main, 5, 0);
  lv_obj_set_style_pad_row(screen_main, 3, 0);
  lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header_cont = lv_obj_create(screen_main);
  lv_obj_set_width(header_cont, LV_PCT(100));
  lv_obj_set_height(header_cont, LV_SIZE_CONTENT);
  lv_obj_add_style(header_cont, &UI::style_card, 0);
  lv_obj_set_style_pad_ver(header_cont, 5, 0);
  lv_obj_t *header_label = lv_label_create(header_cont);
  lv_label_set_text_fmt(header_label, LV_SYMBOL_SETTINGS " %s",
                        FirmwareInfo::DISPLAY_NAME);
  lv_obj_add_style(header_label, &UI::style_header, 0);
  lv_obj_center(header_label);

  lv_obj_t *main_cont = lv_obj_create(screen_main);
  lv_obj_set_width(main_cont, LV_PCT(100));
  lv_obj_set_flex_grow(main_cont, 1);
  lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(main_cont, 0, 0);

  lv_obj_t *btn_config = lv_btn_create(main_cont);
  lv_obj_set_size(btn_config, 150, 90);
  lv_obj_add_style(btn_config, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_config, (void *)"SELECCION_MODO");
  lv_obj_add_event_cb(btn_config, UIHandlers::btn_navegacion_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_flex_flow(btn_config, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn_config, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(btn_config, 8, 0);
  lv_obj_t *config_icon = lv_label_create(btn_config);
  lv_label_set_text(config_icon, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_font(config_icon, &lv_font_montserrat_24, 0);
  lv_obj_t *config_label = lv_label_create(btn_config);
  lv_label_set_text(config_label, "CONFIGURAR");
  lv_obj_set_style_text_font(config_label, &lv_font_montserrat_14, 0);

  lv_obj_t *btn_bobinado = lv_btn_create(main_cont);
  lv_obj_set_size(btn_bobinado, 150, 90);
  lv_obj_add_style(btn_bobinado, &UI::style_btn_success, 0);
  lv_obj_set_user_data(btn_bobinado, (void *)"BOBINADO");
  lv_obj_add_event_cb(btn_bobinado, UIHandlers::btn_navegacion_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_flex_flow(btn_bobinado, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn_bobinado, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(btn_bobinado, 8, 0);
  lv_obj_t *bobinado_icon = lv_label_create(btn_bobinado);
  lv_label_set_text(bobinado_icon, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_font(bobinado_icon, &lv_font_montserrat_24, 0);
  lv_obj_t *bobinado_label = lv_label_create(btn_bobinado);
  lv_label_set_text(bobinado_label, "BOBINAR");
  lv_obj_set_style_text_font(bobinado_label, &lv_font_montserrat_14, 0);

  lv_obj_t *footer_cont = lv_obj_create(screen_main);
  lv_obj_set_width(footer_cont, LV_PCT(100));
  lv_obj_set_height(footer_cont, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(footer_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(footer_cont, 0, 0);
  lv_obj_set_layout(footer_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(footer_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(footer_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *btn_manual = lv_btn_create(footer_cont);
  lv_obj_set_size(btn_manual, 130, 45);
  lv_obj_add_style(btn_manual, &UI::style_btn_warning, 0);
  lv_obj_set_user_data(btn_manual, (void *)"MANUAL");
  lv_obj_add_event_cb(btn_manual, UIHandlers::btn_navegacion_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *label_manual = lv_label_create(btn_manual);
  lv_label_set_text(label_manual, LV_SYMBOL_GPS " Manual");
  lv_obj_center(label_manual);

  lv_obj_t *btn_home = lv_btn_create(footer_cont);
  lv_obj_set_size(btn_home, 130, 45);
  lv_obj_add_style(btn_home, &UI::style_btn_warning, 0);
  lv_obj_set_user_data(btn_home, (void *)"HOME");
  lv_obj_add_event_cb(btn_home, UIHandlers::btn_comando_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *label_home = lv_label_create(btn_home);
  lv_label_set_text(label_home, LV_SYMBOL_HOME " HOME");
  lv_obj_center(label_home);

  lv_obj_t *btn_settings = lv_btn_create(footer_cont);
  lv_obj_set_size(btn_settings, 130, 45);
  lv_obj_add_style(btn_settings, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_settings, (void *)"AJUSTES");
  lv_obj_add_event_cb(btn_settings, UIHandlers::btn_navegacion_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *label_settings = lv_label_create(btn_settings);
  lv_label_set_text(label_settings, LV_SYMBOL_SETTINGS " Ajustes");
  lv_obj_center(label_settings);
}

void crear_pantalla_seleccion_modo() {
  screen_modo_selection = lv_obj_create(NULL);
  lv_obj_add_style(screen_modo_selection, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_modo_selection, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_modo_selection, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_modo_selection, 5, 0);
  lv_obj_set_style_pad_row(screen_modo_selection, 5, 0);
  lv_obj_clear_flag(screen_modo_selection, LV_OBJ_FLAG_SCROLLABLE);

  UI::create_header(screen_modo_selection, "  SELECCIONE MODO",
                    UIHandlers::btn_navegacion_handler);

  lv_obj_t *modes_cont = lv_obj_create(screen_modo_selection);
  lv_obj_set_width(modes_cont, LV_PCT(100));
  lv_obj_set_flex_grow(modes_cont, 1);
  lv_obj_set_layout(modes_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(modes_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(modes_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(modes_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(modes_cont, 0, 0);

  lv_obj_t *btn_transformador = lv_btn_create(modes_cont);
  lv_obj_set_size(btn_transformador, 340, 70);
  lv_obj_add_style(btn_transformador, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_transformador, (void *)"MODO_TRANSFORMADOR");
  lv_obj_add_event_cb(btn_transformador, UIHandlers::btn_modo_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *trans_cont = lv_obj_create(btn_transformador);
  lv_obj_set_size(trans_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(trans_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(trans_cont, 0, 0);
  lv_obj_set_style_pad_all(trans_cont, 0, 0);
  lv_obj_add_flag(trans_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(trans_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_layout(trans_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(trans_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(trans_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(trans_cont, 15, 0);
  lv_obj_set_style_pad_gap(trans_cont, 12, 0);

  lv_obj_t *trans_icon = lv_label_create(trans_cont);
  lv_label_set_text(trans_icon, LV_SYMBOL_LOOP);
  lv_obj_set_style_text_font(trans_icon, &lv_font_montserrat_28, 0);
  lv_obj_add_flag(trans_icon, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(trans_icon, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *trans_text_cont = lv_obj_create(trans_cont);
  lv_obj_set_size(trans_text_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(trans_text_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(trans_text_cont, 0, 0);
  lv_obj_set_style_pad_all(trans_text_cont, 0, 0);
  lv_obj_add_flag(trans_text_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(trans_text_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(trans_text_cont, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *trans_title = lv_label_create(trans_text_cont);
  lv_label_set_text(trans_title, "TRANSFORMADOR");
  lv_obj_set_style_text_font(trans_title, &lv_font_montserrat_16, 0);

  lv_obj_t *trans_desc = lv_label_create(trans_text_cont);
  lv_label_set_text(trans_desc, "Bobinado tradicional capa por capa");
  lv_obj_set_style_text_font(trans_desc, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(trans_desc, lv_color_hex(0x000000), 0);

  lv_obj_t *btn_honeycomb = lv_btn_create(modes_cont);
  lv_obj_set_size(btn_honeycomb, 340, 70);
  lv_obj_add_style(btn_honeycomb, &UI::style_btn_honeycomb, 0);
  lv_obj_set_user_data(btn_honeycomb, (void *)"MODO_HONEYCOMB");
  lv_obj_add_event_cb(btn_honeycomb, UIHandlers::btn_modo_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *hc_cont = lv_obj_create(btn_honeycomb);
  lv_obj_set_size(hc_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(hc_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(hc_cont, 0, 0);
  lv_obj_set_style_pad_all(hc_cont, 0, 0);
  lv_obj_add_flag(hc_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(hc_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_layout(hc_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(hc_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hc_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(hc_cont, 15, 0);
  lv_obj_set_style_pad_gap(hc_cont, 12, 0);

  lv_obj_t *hc_icon = lv_label_create(hc_cont);
  lv_label_set_text(hc_icon, LV_SYMBOL_IMAGE);
  lv_obj_set_style_text_font(hc_icon, &lv_font_montserrat_28, 0);

  lv_obj_t *hc_text_cont = lv_obj_create(hc_cont);
  lv_obj_set_size(hc_text_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(hc_text_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(hc_text_cont, 0, 0);
  lv_obj_set_style_pad_all(hc_text_cont, 0, 0);
  lv_obj_add_flag(hc_text_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(hc_text_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(hc_text_cont, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *hc_title = lv_label_create(hc_text_cont);
  lv_label_set_text(hc_title, "NIDO DE ABEJA");
  lv_obj_set_style_text_font(hc_title, &lv_font_montserrat_16, 0);

  lv_obj_t *hc_desc = lv_label_create(hc_text_cont);
  lv_label_set_text(hc_desc, "Patron entrecruzado con desfase angular");
  lv_obj_set_style_text_font(hc_desc, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hc_desc, UI::color_bg_dark, 0);
}

void crear_pantalla_configuracion() {
  if (screen_config) {
    return;
  }

  screen_config = lv_obj_create(NULL);
  lv_obj_add_style(screen_config, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_config, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_config, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_config, 5, 0);
  lv_obj_set_style_pad_row(screen_config, 5, 0);
  lv_obj_clear_flag(screen_config, LV_OBJ_FLAG_SCROLLABLE);

  UI::create_header(screen_config, LV_SYMBOL_EDIT " TRANSFORMADOR",
                    UIHandlers::back_and_save_handler,
                    UIHandlers::validate_and_wind_handler,
                    "Bobinar " LV_SYMBOL_RIGHT);

  lv_obj_t *params_cont = lv_obj_create(screen_config);
  lv_obj_set_width(params_cont, LV_PCT(100));
  lv_obj_add_style(params_cont, &UI::style_card, 0);
  lv_obj_set_flex_grow(params_cont, 1);
  lv_obj_set_layout(params_cont, LV_LAYOUT_GRID);

  static lv_coord_t col_dsc[] = {LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_FR(1),
                                 LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                 LV_GRID_FR(1), LV_GRID_FR(1),
                                 LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(params_cont, col_dsc, row_dsc);
  lv_obj_set_style_pad_row(params_cont, 5, 0);

  auto create_param_row = [&](int r, const char *l, const char *v,
                              const char *u) -> lv_obj_t * {
    lv_obj_t *lbl = lv_label_create(params_cont);
    lv_label_set_text(lbl, l);
    lv_obj_set_style_text_color(lbl, UI::color_text, 0);
    lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_t *ta = lv_textarea_create(params_cont);
    lv_textarea_set_text(ta, v);
    lv_textarea_set_one_line(ta, true);
    lv_obj_add_style(ta, &UI::style_ta, 0);
    lv_obj_add_event_cb(ta, UIHandlers::ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ta, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_t *ulbl = lv_label_create(params_cont);
    lv_label_set_text(ulbl, u);
    lv_obj_set_style_text_color(ulbl, UI::color_text_secondary, 0);
    lv_obj_set_grid_cell(ulbl, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_set_user_data(ta, (void *)l);
    return ta;
  };

  char buf_diam[16], buf_long[16], buf_vueltas[16], buf_vel[16];
  snprintf(buf_diam, sizeof(buf_diam), "%.2f",
           Sistema::config_transformador.diametro_alambre_mm);
  snprintf(buf_long, sizeof(buf_long), "%.1f",
           Sistema::config_transformador.longitud_bobinado_mm);
  snprintf(buf_vueltas, sizeof(buf_vueltas), "%lu",
           Sistema::config_transformador.vueltas_total);
  snprintf(buf_vel, sizeof(buf_vel), "%.0f",
           Sistema::config_transformador.velocidad_rpm);

  ta_diametro_alambre = create_param_row(0, "Diametro hilo", buf_diam, "mm");
  ta_longitud_bobinado = create_param_row(1, "Ancho carrete", buf_long, "mm");
  ta_vueltas_total = create_param_row(2, "Vueltas totales", buf_vueltas, "");
  ta_velocidad_rpm = create_param_row(3, "Velocidad (Y)", buf_vel, "RPM");

  lv_obj_t *label_switch = lv_label_create(params_cont);
  lv_label_set_text(label_switch, "Pausar en cada capa");
  lv_obj_set_style_text_color(label_switch, UI::color_text, 0);
  lv_obj_set_grid_cell(label_switch, LV_GRID_ALIGN_START, 0, 1,
                       LV_GRID_ALIGN_CENTER, 4, 1);

  sw_detener_en_capas = lv_switch_create(params_cont);
  lv_obj_set_style_bg_color(sw_detener_en_capas, UI::color_primary,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (Sistema::config_transformador.detener_en_capas) {
    lv_obj_add_state(sw_detener_en_capas, LV_STATE_CHECKED);
  }
  lv_obj_set_grid_cell(sw_detener_en_capas, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 4, 1);
}

void crear_pantalla_configuracion_honeycomb() {
  if (screen_config_honeycomb) {
    return;
  }

  screen_config_honeycomb = lv_obj_create(NULL);
  lv_obj_add_style(screen_config_honeycomb, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_config_honeycomb, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_config_honeycomb, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_config_honeycomb, 5, 0);
  lv_obj_set_style_pad_row(screen_config_honeycomb, 5, 0);
  lv_obj_clear_flag(screen_config_honeycomb, LV_OBJ_FLAG_SCROLLABLE);

  UI::create_header(screen_config_honeycomb,
                    LV_SYMBOL_IMAGE " CONFIG NIDO ABEJA",
                    UIHandlers::back_and_save_honeycomb_handler,
                    UIHandlers::validate_and_wind_honeycomb_handler,
                    "Bobinar " LV_SYMBOL_RIGHT);

  lv_obj_t *params_cont = lv_obj_create(screen_config_honeycomb);
  lv_obj_set_width(params_cont, LV_PCT(100));
  lv_obj_add_style(params_cont, &UI::style_card, 0);
  lv_obj_set_flex_grow(params_cont, 1);
  lv_obj_set_layout(params_cont, LV_LAYOUT_GRID);

  static lv_coord_t col_dsc_hc[] = {LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_FR(1),
                                    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc_hc[] = {
      LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),        LV_GRID_FR(1),
      LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(params_cont, col_dsc_hc, row_dsc_hc);
  lv_obj_set_style_pad_row(params_cont, 5, 0);

  auto create_param_row = [&](int r, const char *l, const char *v,
                              const char *u) -> lv_obj_t * {
    lv_obj_t *lbl = lv_label_create(params_cont);
    lv_label_set_text(lbl, l);
    lv_obj_set_style_text_color(lbl, UI::color_text, 0);
    lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_t *ta = lv_textarea_create(params_cont);
    lv_textarea_set_text(ta, v);
    lv_textarea_set_one_line(ta, true);
    lv_obj_add_style(ta, &UI::style_ta, 0);
    lv_obj_add_event_cb(ta, UIHandlers::ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ta, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_t *ulbl = lv_label_create(params_cont);
    lv_label_set_text(ulbl, u);
    lv_obj_set_style_text_color(ulbl, UI::color_text_secondary, 0);
    lv_obj_set_grid_cell(ulbl, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER,
                         r, 1);

    lv_obj_set_user_data(ta, (void *)l);
    return ta;
  };

  char buf_hilo[16], buf_carrete[16], buf_ancho[16], buf_grados[16],
      buf_vueltas[16], buf_vel[16];
  snprintf(buf_hilo, sizeof(buf_hilo), "%.2f",
           Sistema::config_nido_abeja.diametro_hilo);
  snprintf(buf_carrete, sizeof(buf_carrete), "%.1f",
           Sistema::config_nido_abeja.diametro_carrete);
  snprintf(buf_ancho, sizeof(buf_ancho), "%.1f",
           Sistema::config_nido_abeja.ancho_carrete);
  snprintf(buf_grados, sizeof(buf_grados), "%.1f",
           Sistema::config_nido_abeja.desfase_grados);
  snprintf(buf_vueltas, sizeof(buf_vueltas), "%lu",
           Sistema::config_nido_abeja.num_vueltas);
  snprintf(buf_vel, sizeof(buf_vel), "%.0f",
           Sistema::config_nido_abeja.velocidad_rpm);

  ta_hc_diametro_hilo = create_param_row(0, "Diametro hilo", buf_hilo, "mm");
  ta_hc_diametro_carrete =
      create_param_row(1, "Diam. carrete", buf_carrete, "mm");
  ta_hc_ancho_carrete = create_param_row(2, "Ancho carrete", buf_ancho, "mm");
  ta_hc_desfase_grados = create_param_row(3, "Desfase", buf_grados, "°");
  ta_hc_num_vueltas = create_param_row(4, "Num. vueltas", buf_vueltas, "");
  ta_hc_velocidad = create_param_row(5, "Velocidad", buf_vel, "RPM");
}

void destruir_pantalla_configuracion() {
  if (!screen_config) {
    return;
  }

  lv_obj_t *screen_to_delete = screen_config;
  screen_config = nullptr;
  ta_diametro_alambre = nullptr;
  ta_longitud_bobinado = nullptr;
  ta_vueltas_total = nullptr;
  ta_velocidad_rpm = nullptr;
  sw_detener_en_capas = nullptr;
  lv_obj_delete_async(screen_to_delete);
}

void destruir_pantalla_configuracion_honeycomb() {
  if (!screen_config_honeycomb) {
    return;
  }

  lv_obj_t *screen_to_delete = screen_config_honeycomb;
  screen_config_honeycomb = nullptr;
  ta_hc_diametro_hilo = nullptr;
  ta_hc_diametro_carrete = nullptr;
  ta_hc_ancho_carrete = nullptr;
  ta_hc_desfase_grados = nullptr;
  ta_hc_num_vueltas = nullptr;
  ta_hc_velocidad = nullptr;
  lv_obj_delete_async(screen_to_delete);
}

void crear_pantalla_bobinado() {
  screen_winding = lv_obj_create(NULL);
  lv_obj_add_style(screen_winding, &UI::style_main_bg, 0);
  lv_obj_add_event_cb(screen_winding, UIHandlers::screen_winding_load_handler,
                      LV_EVENT_SCREEN_LOADED, NULL);
  lv_obj_set_layout(screen_winding, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_winding, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_winding, 5, 0);
  lv_obj_set_style_pad_row(screen_winding, 3, 0);
  lv_obj_clear_flag(screen_winding, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header_cont = lv_obj_create(screen_winding);
  lv_obj_set_width(header_cont, LV_PCT(100));
  lv_obj_set_height(header_cont, LV_SIZE_CONTENT);
  lv_obj_add_style(header_cont, &UI::style_card, 0);
  lv_obj_set_style_pad_ver(header_cont, 6, 0);
  lv_obj_set_layout(header_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *btn_home = lv_btn_create(header_cont);
  lv_obj_set_size(btn_home, 40, 40);
  lv_obj_add_style(btn_home, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_home, (void *)"PRINCIPAL");
  lv_obj_add_event_cb(btn_home, UIHandlers::btn_navegacion_handler,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_t *label_home = lv_label_create(btn_home);
  lv_label_set_text(label_home, LV_SYMBOL_HOME);
  lv_obj_center(label_home);

  lv_obj_t *header_title = lv_label_create(header_cont);
  lv_label_set_text(header_title, "CONTROL DE BOBINADO");
  lv_obj_add_style(header_title, &UI::style_header, 0);
  lv_obj_set_flex_grow(header_title, 1);
  lv_obj_set_style_text_align(header_title, LV_TEXT_ALIGN_CENTER, 0);

  label_modo_actual = lv_label_create(header_cont);
  lv_label_set_text(label_modo_actual, LV_SYMBOL_LOOP " Transformador");
  lv_obj_set_style_text_font(label_modo_actual, &lv_font_montserrat_14, 0);

  label_estado = lv_label_create(header_cont);
  lv_label_set_text(label_estado, "LISTO");
  lv_obj_set_style_text_font(label_estado, &lv_font_montserrat_16, 0);

  lv_obj_t *info_panel = lv_obj_create(screen_winding);
  lv_obj_set_width(info_panel, LV_PCT(100));
  lv_obj_add_style(info_panel, &UI::style_card, 0);
  lv_obj_set_style_pad_ver(info_panel, 6, 0);
  lv_obj_set_style_pad_hor(info_panel, 10, 0);
  lv_obj_set_flex_grow(info_panel, 1);
  lv_obj_set_flex_flow(info_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info_panel, LV_FLEX_ALIGN_SPACE_AROUND,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *vueltas_cont = lv_obj_create(info_panel);
  lv_obj_set_size(vueltas_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(vueltas_cont, 0, 0);
  lv_obj_set_flex_flow(vueltas_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(vueltas_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_END);
  lv_obj_set_style_bg_opa(vueltas_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(vueltas_cont, 0, 0);
  label_vueltas_actuales = lv_label_create(vueltas_cont);
  lv_label_set_text(label_vueltas_actuales, "0");
  lv_obj_add_style(label_vueltas_actuales, &UI::style_label_value, 0);
  label_vueltas_totales = lv_label_create(vueltas_cont);
  lv_label_set_text(label_vueltas_totales, "/ 0");
  lv_obj_set_style_text_font(label_vueltas_totales, &lv_font_montserrat_20, 0);
  lv_obj_set_style_pad_bottom(label_vueltas_totales, 5, 0);
  lv_obj_set_style_text_color(label_vueltas_totales, UI::color_text_secondary,
                              0);

  lv_obj_t *prog_cont = lv_obj_create(info_panel);
  lv_obj_set_width(prog_cont, LV_PCT(95));
  lv_obj_set_height(prog_cont, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(prog_cont, 0, 0);
  lv_obj_set_layout(prog_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(prog_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(prog_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(prog_cont, 0, 0);
  bar_progreso = lv_bar_create(prog_cont);
  lv_obj_set_width(bar_progreso, LV_PCT(85));
  lv_obj_set_height(bar_progreso, 15);
  label_progreso = lv_label_create(prog_cont);
  lv_label_set_text(label_progreso, "0%");
  lv_obj_set_style_text_font(label_progreso, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label_progreso, UI::color_text_secondary, 0);

  lv_obj_t *extra_info_cont = lv_obj_create(info_panel);
  lv_obj_set_width(extra_info_cont, LV_PCT(100));
  lv_obj_set_height(extra_info_cont, LV_SIZE_CONTENT);
  lv_obj_set_layout(extra_info_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(extra_info_cont, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(extra_info_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(extra_info_cont, 0, 0);
  label_capa_info = lv_label_create(extra_info_cont);
  lv_label_set_text(label_capa_info,
                    "Capa: 0/0 | Vueltas: 0/0 | Grosor: 0.00 mm");
  lv_obj_add_style(label_capa_info, &UI::style_text_secondary, 0);

  lv_obj_t *sec_panel = lv_obj_create(info_panel);
  lv_obj_set_width(sec_panel, LV_PCT(100));
  lv_obj_set_height(sec_panel, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(sec_panel, 0, 0);
  lv_obj_set_layout(sec_panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(sec_panel, LV_FLEX_ALIGN_SPACE_AROUND,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(sec_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sec_panel, 0, 0);
  label_velocidad_actual = lv_label_create(sec_panel);
  lv_label_set_text(label_velocidad_actual, LV_SYMBOL_CHARGE " 0 RPM");
  lv_obj_set_style_text_color(label_velocidad_actual, UI::color_text, 0);
  label_tiempo_restante = lv_label_create(sec_panel);
  lv_label_set_text(label_tiempo_restante, LV_SYMBOL_POWER " --:--");
  lv_obj_set_style_text_color(label_tiempo_restante, UI::color_text, 0);

  lv_obj_t *ctrl_panel = lv_obj_create(screen_winding);
  lv_obj_set_width(ctrl_panel, LV_PCT(100));
  lv_obj_set_height(ctrl_panel, LV_SIZE_CONTENT);
  lv_obj_add_style(ctrl_panel, &UI::style_card, 0);
  lv_obj_set_style_pad_ver(ctrl_panel, 6, 0);
  lv_obj_set_layout(ctrl_panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(ctrl_panel, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  struct {
    const char *t;
    const char *id;
    lv_style_t *s;
  } btns[] = {{LV_SYMBOL_PLAY " Iniciar", "INICIAR", &UI::style_btn_success},
              {LV_SYMBOL_NEXT " Seguir", "PROSEGUIR", &UI::style_btn_primary},
              {LV_SYMBOL_PAUSE " Pausar", "PAUSAR", &UI::style_btn_warning},
              {LV_SYMBOL_STOP " Parar", "PARAR", &UI::style_btn_danger}};
  for (int i = 0; i < 4; i++) {
    lv_obj_t *btn = lv_btn_create(ctrl_panel);
    lv_obj_set_user_data(btn, (void *)btns[i].id);
    lv_obj_set_size(btn, 100, 45);
    lv_obj_add_style(btn, btns[i].s, 0);
    lv_obj_add_event_cb(btn, UIHandlers::btn_comando_handler, LV_EVENT_CLICKED,
                        NULL);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, btns[i].t);
    lv_obj_center(l);
  }
}

void crear_pantalla_control_manual() {
  screen_manual_control = lv_obj_create(NULL);
  lv_obj_add_style(screen_manual_control, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_manual_control, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_manual_control, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen_manual_control, 5, 0);
  lv_obj_set_style_pad_row(screen_manual_control, 5, 0);
  lv_obj_clear_flag(screen_manual_control, LV_OBJ_FLAG_SCROLLABLE);

  UI::create_header(screen_manual_control, "  " LV_SYMBOL_GPS " CONTROL MANUAL",
                    UIHandlers::btn_manual_back_handler);

  lv_obj_t *warning_card = lv_obj_create(screen_manual_control);
  lv_obj_set_width(warning_card, LV_PCT(100));
  lv_obj_set_height(warning_card, LV_SIZE_CONTENT);
  lv_obj_add_style(warning_card, &UI::style_card, 0);
  lv_obj_set_style_bg_color(warning_card, UI::color_warning, 0);
  lv_obj_t *warning = lv_label_create(warning_card);
  lv_label_set_text(warning, LV_SYMBOL_WARNING
                    " Disponible solo en estado LISTO o PAUSADO");
  lv_obj_center(warning);
  lv_obj_set_style_text_color(warning, UI::color_bg_dark, 0);
  lv_obj_set_style_text_font(warning, &lv_font_montserrat_12, 0);

  lv_obj_t *axes_panel = lv_obj_create(screen_manual_control);
  lv_obj_set_width(axes_panel, LV_PCT(100));
  lv_obj_set_flex_grow(axes_panel, 1);
  lv_obj_add_style(axes_panel, &UI::style_card, 0);
  lv_obj_set_flex_flow(axes_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(axes_panel, LV_FLEX_ALIGN_SPACE_AROUND,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *x_cont = lv_obj_create(axes_panel);
  lv_obj_set_width(x_cont, LV_PCT(100));
  lv_obj_set_height(x_cont, LV_SIZE_CONTENT);
  lv_obj_set_layout(x_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(x_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(x_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(x_cont, 0, 0);
  lv_obj_t *label_x = lv_label_create(x_cont);
  lv_label_set_text(label_x, "EJE X:");
  lv_obj_set_style_text_color(label_x, UI::color_primary, 0);

  lv_obj_t *btn_x_minus = UI::create_button(
      x_cont, LV_SYMBOL_LEFT " -0.5mm", "X-", &UI::style_btn_warning,
      UIHandlers::btn_manual_handler, 150, 40);
  lv_obj_add_event_cb(btn_x_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(btn_x_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_add_event_cb(btn_x_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_RELEASED, NULL);

  lv_obj_t *btn_x_plus = UI::create_button(
      x_cont, "+0.5mm " LV_SYMBOL_RIGHT, "X+", &UI::style_btn_warning,
      UIHandlers::btn_manual_handler, 150, 40);
  lv_obj_add_event_cb(btn_x_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(btn_x_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_add_event_cb(btn_x_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_RELEASED, NULL);

  lv_obj_t *y_cont = lv_obj_create(axes_panel);
  lv_obj_set_width(y_cont, LV_PCT(100));
  lv_obj_set_height(y_cont, LV_SIZE_CONTENT);
  lv_obj_set_layout(y_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(y_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(y_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(y_cont, 0, 0);
  lv_obj_t *label_y = lv_label_create(y_cont);
  lv_label_set_text(label_y, "EJE Y:");
  lv_obj_set_style_text_color(label_y, UI::color_success, 0);

  lv_obj_t *btn_y_minus = UI::create_button(
      y_cont, LV_SYMBOL_MINUS " -0.25v", "Y-", &UI::style_btn_warning,
      UIHandlers::btn_manual_handler, 150, 40);
  lv_obj_add_event_cb(btn_y_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(btn_y_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_add_event_cb(btn_y_minus, UIHandlers::btn_manual_handler,
                      LV_EVENT_RELEASED, NULL);

  lv_obj_t *btn_y_plus = UI::create_button(
      y_cont, LV_SYMBOL_PLUS " +0.25v", "Y+", &UI::style_btn_warning,
      UIHandlers::btn_manual_handler, 150, 40);
  lv_obj_add_event_cb(btn_y_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(btn_y_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  lv_obj_add_event_cb(btn_y_plus, UIHandlers::btn_manual_handler,
                      LV_EVENT_RELEASED, NULL);
}

void crear_pantalla_ajustes() {
  screen_settings = lv_obj_create(NULL);
  lv_obj_add_style(screen_settings, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_settings, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_settings, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen_settings, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen_settings, 5, 0);
  lv_obj_set_style_pad_row(screen_settings, 12, 0);
  lv_obj_clear_flag(screen_settings, LV_OBJ_FLAG_SCROLLABLE);

  UI::create_header(screen_settings, LV_SYMBOL_SETTINGS " AJUSTES",
                    UIHandlers::btn_navegacion_handler);

  lv_obj_t *card = UI::create_card(screen_settings);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_pad_row(card, 10, 0);

  UI::create_button(card, "Abrir retroiluminacion", "AJUSTES_BRILLO",
                    &UI::style_btn_primary,
                    UIHandlers::btn_navegacion_handler, 300, 38);
  UI::create_button(card, "Informacion del sistema", "AJUSTES_INFO",
                    &UI::style_btn_primary,
                    UIHandlers::btn_navegacion_handler, 300, 38);
  UI::create_button(card, "Estado de la maquina", "AJUSTES_ESTADO",
                    &UI::style_btn_primary,
                    UIHandlers::btn_navegacion_handler, 300, 38);
  UI::create_button(card, "Actualizar firmware", "AJUSTES_FIRMWARE",
                    &UI::style_btn_warning,
                    UIHandlers::btn_navegacion_handler, 300, 38);
}

void crear_pantalla_ajustes_brillo() {
  if (screen_settings_brightness) {
    return;
  }

  screen_settings_brightness = lv_obj_create(NULL);
  lv_obj_add_event_cb(screen_settings_brightness,
                      brightness_submenu_screen_event_cb,
                      LV_EVENT_SCREEN_LOADED, NULL);
  lv_obj_add_style(screen_settings_brightness, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_settings_brightness, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_settings_brightness, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen_settings_brightness, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen_settings_brightness, 8, 0);
  lv_obj_set_style_pad_row(screen_settings_brightness, 16, 0);
  lv_obj_clear_flag(screen_settings_brightness, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(screen_settings_brightness);
  lv_label_set_text(title, "Retroiluminacion");
  lv_obj_add_style(title, &UI::style_header, 0);

  label_brightness_submenu_value = lv_label_create(screen_settings_brightness);
  lv_obj_set_style_text_color(label_brightness_submenu_value, UI::color_text,
                              0);
  lv_obj_set_style_text_font(label_brightness_submenu_value,
                             &lv_font_montserrat_16, 0);

  slider_brightness_submenu = lv_slider_create(screen_settings_brightness);
  lv_obj_set_width(slider_brightness_submenu, 260);
  lv_slider_set_range(slider_brightness_submenu, 15, 255);
  lv_obj_add_event_cb(slider_brightness_submenu,
                      brightness_submenu_slider_changed_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(slider_brightness_submenu,
                      brightness_submenu_slider_released_cb, LV_EVENT_RELEASED,
                      NULL);

  lv_obj_t *btn_back = lv_btn_create(screen_settings_brightness);
  lv_obj_set_size(btn_back, 140, 45);
  lv_obj_add_style(btn_back, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_back, (void *)"CERRAR_AJUSTES_BRILLO");
  lv_obj_add_event_cb(btn_back, settings_submenu_back_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_back = lv_label_create(btn_back);
  lv_label_set_text(label_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_center(label_back);

  sync_brightness_submenu_controls();
}

void crear_pantalla_ajustes_info() {
  if (screen_settings_info) {
    update_system_info_labels();
    return;
  }

  screen_settings_info = lv_obj_create(NULL);
  lv_obj_add_event_cb(screen_settings_info, settings_info_screen_event_cb,
                      LV_EVENT_SCREEN_LOADED, NULL);
  lv_obj_add_style(screen_settings_info, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_settings_info, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_settings_info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen_settings_info, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen_settings_info, 8, 0);
  lv_obj_set_style_pad_row(screen_settings_info, 10, 0);
  lv_obj_clear_flag(screen_settings_info, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(screen_settings_info);
  lv_label_set_text(title, "Informacion del sistema");
  lv_obj_add_style(title, &UI::style_header, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  lv_obj_t *info_grid = lv_obj_create(screen_settings_info);
  lv_obj_set_width(info_grid, LV_PCT(100));
  lv_obj_set_style_bg_opa(info_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(info_grid, 0, 0);
  lv_obj_set_style_pad_all(info_grid, 4, 0);
  lv_obj_set_style_pad_row(info_grid, 6, 0);
  lv_obj_set_style_pad_column(info_grid, 8, 0);
  lv_obj_set_layout(info_grid, LV_LAYOUT_GRID);

  static lv_coord_t info_cols[] = {LV_GRID_FR(2), LV_GRID_FR(3),
                                   LV_GRID_TEMPLATE_LAST};
  static lv_coord_t info_rows[] = {
      LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
      LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(info_grid, info_cols, info_rows);

  auto create_info_row = [&](int row_index,
                             const char *label_text) -> lv_obj_t * {
    lv_obj_t *label = lv_label_create(info_grid);
    lv_label_set_text(label, label_text);
    lv_obj_add_style(label, &UI::style_text_secondary, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 1,
                         LV_GRID_ALIGN_CENTER, row_index, 1);

    lv_obj_t *value = lv_label_create(info_grid);
    lv_obj_set_style_text_color(value, UI::color_text, 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(value, LV_PCT(100));
    lv_obj_set_grid_cell(value, LV_GRID_ALIGN_STRETCH, 1, 1,
                         LV_GRID_ALIGN_CENTER, row_index, 1);
    return value;
  };

  label_info_display_name = create_info_row(0, "Firmware");
  label_info_codename = create_info_row(1, "Codename");
  label_info_version = create_info_row(2, "Version");
  label_info_brightness = create_info_row(3, "Brillo actual");
  label_info_mode = create_info_row(4, "Modo actual");
  label_info_status = create_info_row(5, "Estado actual");

  lv_obj_t *btn_back = lv_btn_create(screen_settings_info);
  lv_obj_set_size(btn_back, 140, 45);
  lv_obj_add_style(btn_back, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_back, (void *)"CERRAR_AJUSTES_INFO");
  lv_obj_add_event_cb(btn_back, settings_submenu_back_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_back = lv_label_create(btn_back);
  lv_label_set_text(label_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_center(label_back);

  update_system_info_labels();
}

void crear_pantalla_ajustes_estado() {
  if (screen_settings_machine_status) {
    update_system_info_labels();
    return;
  }

  screen_settings_machine_status = lv_obj_create(NULL);
  lv_obj_add_event_cb(screen_settings_machine_status,
                      settings_machine_status_screen_event_cb,
                      LV_EVENT_SCREEN_LOADED, NULL);
  lv_obj_add_style(screen_settings_machine_status, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_settings_machine_status, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_settings_machine_status, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen_settings_machine_status, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen_settings_machine_status, 8, 0);
  lv_obj_set_style_pad_row(screen_settings_machine_status, 14, 0);
  lv_obj_clear_flag(screen_settings_machine_status, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(screen_settings_machine_status);
  lv_label_set_text(title, "Estado de la maquina");
  lv_obj_add_style(title, &UI::style_header, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  lv_obj_t *status_card = UI::create_card(screen_settings_machine_status);
  lv_obj_set_width(status_card, LV_PCT(100));
  lv_obj_set_height(status_card, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(status_card, 10, 0);

  label_machine_status_summary = lv_label_create(status_card);
  lv_obj_set_width(label_machine_status_summary, LV_PCT(100));
  lv_label_set_long_mode(label_machine_status_summary, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(label_machine_status_summary, UI::color_text, 0);
  lv_obj_set_style_text_font(label_machine_status_summary,
                             &lv_font_montserrat_12, 0);

  lv_obj_t *btn_back = lv_btn_create(screen_settings_machine_status);
  lv_obj_set_size(btn_back, 140, 45);
  lv_obj_add_style(btn_back, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_back, (void *)"CERRAR_AJUSTES_ESTADO");
  lv_obj_add_event_cb(btn_back, settings_submenu_back_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_back = lv_label_create(btn_back);
  lv_label_set_text(label_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_center(label_back);

  update_system_info_labels();
}

void crear_pantalla_ajustes_firmware() {
  if (screen_settings_firmware) {
    return;
  }

  screen_settings_firmware = lv_obj_create(NULL);
  lv_obj_add_style(screen_settings_firmware, &UI::style_main_bg, 0);
  lv_obj_set_layout(screen_settings_firmware, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screen_settings_firmware, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen_settings_firmware, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen_settings_firmware, 8, 0);
  lv_obj_set_style_pad_row(screen_settings_firmware, 14, 0);
  lv_obj_add_flag(screen_settings_firmware, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(screen_settings_firmware);
  lv_label_set_text(title, "Actualizar firmware");
  lv_obj_add_style(title, &UI::style_header, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  lv_obj_t *source_label = lv_label_create(screen_settings_firmware);
  lv_label_set_text(source_label, "Fuente: tarjeta SD");
  lv_obj_add_style(source_label, &UI::style_text_secondary, 0);

  firmware_version_dropdown = lv_dropdown_create(screen_settings_firmware);
  lv_obj_set_width(firmware_version_dropdown, 300);
  lv_dropdown_set_options(firmware_version_dropdown, "Buscando versiones...");

  firmware_status_label = lv_label_create(screen_settings_firmware);
  lv_obj_set_width(firmware_status_label, 300);
  lv_label_set_long_mode(firmware_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(firmware_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(firmware_status_label, "Preparando tarjeta SD...");

  firmware_progress_bar = lv_bar_create(screen_settings_firmware);
  lv_obj_set_width(firmware_progress_bar, 300);
  lv_bar_set_range(firmware_progress_bar, 0, 100);
  lv_bar_set_value(firmware_progress_bar, 0, LV_ANIM_OFF);

  firmware_install_button = UI::create_button(
      screen_settings_firmware, "Instalar seleccionada", "FIRMWARE_INSTALAR",
      &UI::style_btn_success, firmware_install_button_cb, 300, 40);
  lv_obj_add_state(firmware_install_button, LV_STATE_DISABLED);

  firmware_restart_button = UI::create_button(
      screen_settings_firmware, "Reiniciar para aplicar", "FIRMWARE_REINICIAR",
      &UI::style_btn_primary, firmware_restart_button_cb, 300, 40);
  lv_obj_add_state(firmware_restart_button, LV_STATE_DISABLED);

  firmware_update_timer = lv_timer_create(firmware_update_timer_cb, 100, NULL);

  UI::create_button(screen_settings_firmware, "Buscar en SD", "FIRMWARE_ESCANEAR",
                    &UI::style_btn_primary, firmware_scan_button_cb, 300, 40);

  lv_obj_t *wifi_label = lv_label_create(screen_settings_firmware);
  lv_label_set_text(wifi_label, "Actualizacion por WiFi: proximamente");
  lv_obj_add_style(wifi_label, &UI::style_text_secondary, 0);

  lv_obj_t *btn_back = lv_btn_create(screen_settings_firmware);
  lv_obj_set_size(btn_back, 140, 45);
  lv_obj_add_style(btn_back, &UI::style_btn_primary, 0);
  lv_obj_set_user_data(btn_back, (void *)"CERRAR_AJUSTES_FIRMWARE");
  lv_obj_add_event_cb(btn_back, settings_submenu_back_handler,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_back = lv_label_create(btn_back);
  lv_label_set_text(label_back, LV_SYMBOL_LEFT " Volver");
  lv_obj_center(label_back);

  refresh_firmware_packages();
}

void init_all_screens() {
  // Montar la SD durante el arranque, antes de que la UI empiece a usarla.
  ProfileManager::init();
  crear_pantalla_principal();
  crear_pantalla_seleccion_modo();
  crear_pantalla_bobinado();
  crear_pantalla_control_manual();
  crear_pantalla_ajustes();
}
} // namespace UIScreens
