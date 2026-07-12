// =========================================================================
// BOBINADORA CNC CON LVGL 9.x - VERSIÓN PLATFORMIO JC4827W543
// VERSIÓN: 10.3 - Estable y Optimizada
// NOTAS DE VERSIÓN 10.3:
// - Corregido error en visualización de vueltas totales (siempre salía 0)
// - Corregido título de modo en pantalla de bobinado
// - Limpieza de código y eliminación de secciones obsoletas
// - Mejora en la estabilidad de la tarea de UI
// =========================================================================

#include <Arduino.h>
#include <Wire.h>
#include <freertos/task.h>
#include <lvgl.h>

// Hardware libraries
#include "display_init.h"
#include "touch_init.h"

// Project headers
#include "config.h"
#include "motor_task_optimized.h"
#include "persistence.h"
#include "profile_manager.h"
#include "ui_components.h"
#include "ui_handlers.h"
#include "ui_screens.h"

// Sistema
ConfigTransformador Sistema::config_transformador;
ConfigNidoAbeja Sistema::config_nido_abeja;
EstadoSistema Sistema::estado;

// LVGL objects
static lv_display_t *disp;
static lv_indev_t *indev;
static lv_color_t *disp_draw_buf;

// Flag para indicar si touch está disponible
static bool touch_available = false;

// Flag para indicar si LVGL está completamente inicializado (PROTECCIÓN CONTRA
// CRASH)
static bool lvgl_ready = false;

// =========================================================================
// VARIABLES PARA DEBUG Y LOGGING (CORE 0)
// =========================================================================
struct DebugInfo {
  unsigned long ultima_vuelta_registrada = 0;
  unsigned long ultima_capa_registrada = 0;
  unsigned long tiempo_ultimo_log = 0;
  bool bobinado_iniciado = false;
};
static DebugInfo debug_info;

// =========================================================================
// FUNCIONES DE HARDWARE (CORE 0)
// =========================================================================
void init_hardware() {
  Serial.println("\n[Hardware] Inicializando pines de motores...");

  pinMode(Hardware::Motor::STEP_X_PIN, OUTPUT);
  pinMode(Hardware::Motor::DIR_X_PIN, OUTPUT);
  pinMode(Hardware::Motor::ENABLE_X_PIN, OUTPUT);
  pinMode(Hardware::Motor::LIMIT_X_PIN, INPUT_PULLUP);
  pinMode(Hardware::Motor::STEP_Y_PIN, OUTPUT);
  pinMode(Hardware::Motor::DIR_Y_PIN, OUTPUT);
  pinMode(Hardware::Motor::ENABLE_Y_PIN, OUTPUT);
  pinMode(Hardware::Motor::LIMIT_Y_PIN, INPUT_PULLUP);
  pinMode(Hardware::Touch::SW1_PIN, INPUT_PULLUP);

  // Deshabilitar motores inicialmente
  digitalWrite(Hardware::Motor::ENABLE_X_PIN, HIGH);
  digitalWrite(Hardware::Motor::ENABLE_Y_PIN, HIGH);

  Serial.println("[Hardware] ✓ Pines de motores configurados");
}

bool UIHandlers::homing_ejes() {
  Serial.println("\n>>> INICIANDO HOMING <<<");
  Sistema::estado.estado = EstadoBobinado::HOMING;
  Sistema::estado.movimiento_manual_activo = true;

  digitalWrite(Hardware::Motor::ENABLE_X_PIN, LOW);
  digitalWrite(Hardware::Motor::ENABLE_Y_PIN, LOW);
  delay(50);

  digitalWrite(Hardware::Motor::DIR_X_PIN, LOW);
  int pasos = 0;
  const int MAX_PASOS = Hardware::Motor::PASOS_POR_MM_X * 500;
  bool limite_detectado = false;

  while (pasos < MAX_PASOS) {
    bool limite = !digitalRead(Hardware::Motor::LIMIT_X_PIN);
    if (limite) {
      Serial.println("Limite detectado!");
      limite_detectado = true;
      break;
    }
    digitalWrite(Hardware::Motor::STEP_X_PIN, HIGH);
    delayMicroseconds(Hardware::Motor::STEP_PULSE_US);
    digitalWrite(Hardware::Motor::STEP_X_PIN, LOW);
    delayMicroseconds(Hardware::Motor::HOMING_SPEED_DELAY);
    pasos++;
  }

  if (!limite_detectado) {
    Serial.println("ERROR: Homing X sin detectar LIMIT_X");
    digitalWrite(Hardware::Motor::ENABLE_X_PIN, HIGH);
    digitalWrite(Hardware::Motor::ENABLE_Y_PIN, HIGH);
    Sistema::estado.estado = EstadoBobinado::ERROR;
    Sistema::estado.movimiento_manual_activo = false;
    Sistema::estado.rpm_objetivo = 0.0f;
    return false;
  }

  digitalWrite(Hardware::Motor::DIR_X_PIN, HIGH);
  for (int i = 0; i < Hardware::Motor::HOMING_BACK_STEPS; i++) {
    digitalWrite(Hardware::Motor::STEP_X_PIN, HIGH);
    delayMicroseconds(Hardware::Motor::STEP_PULSE_US);
    digitalWrite(Hardware::Motor::STEP_X_PIN, LOW);
    delayMicroseconds(Hardware::Motor::HOMING_SPEED_DELAY);
  }

  Sistema::estado.reset();
  Sistema::estado.estado = EstadoBobinado::LISTO;
  Sistema::estado.movimiento_manual_activo = false;
  Serial.println(">>> HOMING COMPLETADO <<<\n");
  return true;
}

void UIHandlers::move_motor_steps_safe(int step_pin, int dir_pin, int steps,
                                       int delay_us) {
  if (steps == 0)
    return;
  Sistema::estado.movimiento_manual_activo = true;
  int enable_pin = (step_pin == Hardware::Motor::STEP_X_PIN)
                       ? Hardware::Motor::ENABLE_X_PIN
                       : Hardware::Motor::ENABLE_Y_PIN;
  bool was_enabled = (digitalRead(enable_pin) == LOW);
  if (!was_enabled) {
    digitalWrite(enable_pin, LOW);
    delayMicroseconds(100);
  }
  digitalWrite(dir_pin, (steps > 0) ? HIGH : LOW);
  steps = abs(steps);
  for (int i = 0; i < steps; i++) {
    digitalWrite(step_pin, HIGH);
    delayMicroseconds(delay_us);
    digitalWrite(step_pin, LOW);
    delayMicroseconds(delay_us);
  }
  if (!was_enabled)
    digitalWrite(enable_pin, HIGH);
  Sistema::estado.movimiento_manual_activo = false;
}

// =========================================================================
// TAREA DE LOGGING Y DEBUG (CORE 0)
// =========================================================================
void logging_task(void *pvParameters) {
  Serial.println("[Logging] Tarea iniciada en Core 0");

  for (;;) {
    unsigned long ahora = millis();

    if (Sistema::estado.estado == EstadoBobinado::BOBINANDO &&
        !debug_info.bobinado_iniciado) {
      Serial.println("\n========================================");
      Serial.println(">>> INICIANDO NUEVO BOBINADO <<<");
      Serial.print("Modo: ");
      Serial.println(Sistema::estado.modo == ModoBobinado::TRANSFORMADOR
                         ? "TRANSFORMADOR"
                         : "NIDO DE ABEJA");

      if (Sistema::estado.modo == ModoBobinado::TRANSFORMADOR) {
        Serial.print("Vueltas totales: ");
        Serial.println(Sistema::config_transformador.vueltas_total);
        Serial.print("Velocidad objetivo: ");
        Serial.print(Sistema::config_transformador.velocidad_rpm);
        Serial.println(" RPM");
      } else {
        Serial.print("Vueltas totales: ");
        Serial.println(Sistema::config_nido_abeja.num_vueltas);
        Serial.print("Desfase: ");
        Serial.print(Sistema::config_nido_abeja.desfase_grados);
        Serial.println("°");
      }
      Serial.println("========================================\n");

      debug_info.bobinado_iniciado = true;
      debug_info.ultima_vuelta_registrada = 0;
      debug_info.ultima_capa_registrada = 0;
    }

    if (Sistema::estado.estado != EstadoBobinado::BOBINANDO &&
        debug_info.bobinado_iniciado) {
      Serial.println("\n========================================");
      Serial.println(">>> BOBINADO FINALIZADO <<<");
      Serial.print("Vueltas completadas: ");
      Serial.println(Sistema::estado.vueltas_completadas);
      Serial.println("========================================\n");

      debug_info.bobinado_iniciado = false;
    }

    if (Sistema::estado.estado == EstadoBobinado::BOBINANDO &&
        (ahora - debug_info.tiempo_ultimo_log) >= 5000) {
      Serial.print(">>> Vuelta ");
      Serial.print(Sistema::estado.vueltas_completadas);
      Serial.print(" | Vel: ");
      Serial.print(Sistema::estado.rpm_actual, 1);
      Serial.println(" RPM");
      debug_info.tiempo_ultimo_log = ahora;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// =========================================================================
// LVGL DRIVERS (LVGL 9.x)
// =========================================================================
uint32_t millis_cb(void) { return millis(); }

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  if (gfx) {
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  }

  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (!touch_available) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int16_t x, y;

  if (touch_read(&x, &y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// =========================================================================
// TAREA DE UI (LVGL) - CORE 0
// =========================================================================
void ui_task(void *pvParameters) {
  Serial.println("[UI] Tarea iniciada en Core 0");

  static bool ultimo_estado_sw1 = HIGH;

  while (1) {
    // Control de retroiluminación con botón físico SW1
    bool estado_actual_sw1 = digitalRead(Hardware::Touch::SW1_PIN);
    if (estado_actual_sw1 == LOW && ultimo_estado_sw1 == HIGH) {
      // Ciclo de brillo: 255 -> 15 -> 120 -> 255
      if (Sistema::estado.brillo_backlight > 200)
        Sistema::estado.brillo_backlight = 15; // Muy bajo, ideal para fotos
      else if (Sistema::estado.brillo_backlight < 50)
        Sistema::estado.brillo_backlight = 120; // Nivel medio-suave
      else
        Sistema::estado.brillo_backlight = 255; // Nivel máximo

      display_backlight(Sistema::estado.brillo_backlight);
      Serial.printf("[Hardware] Botón SW1 presionado. Brillo: %d/255\n",
                    Sistema::estado.brillo_backlight);
      vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
    }
    ultimo_estado_sw1 = estado_actual_sw1;

    if (lvgl_ready) {
      lv_timer_handler();
    }
    vTaskDelay(pdMS_TO_TICKS(Hardware::LVGL_TICK_PERIOD));
  }
}

// =========================================================================
// SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(2000); // Esperar a que Serial esté listo

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("   BOBINADORA CNC v10.3");
  Serial.println("   PlatformIO + LVGL 9.x + JC4827W543");
  Serial.println("========================================");
  Serial.println();

  // Inicializar sistema
  Serial.println("[1/7] Inicializando sistema...");
  Sistema::inicializar();
  init_hardware();
  Persistence.begin();
  Persistence.loadAll();

  // Inicializar Display - CRÍTICO
  Serial.println("\n[2/7] Inicializando display QSPI...");
  if (!display_init()) {
    Serial.println("\n!!! ERROR FATAL: Display no inicializado !!!");
    Serial.println("Verifica las conexiones del display");
    Serial.println("El sistema se detendrá aquí\n");
    while (1) {
      delay(1000);
      Serial.print(".");
    }
  }
  Serial.println("[Display] ✓ OK\n");

  // Inicializar Touch - NO CRÍTICO
  Serial.println("[3/7] Inicializando touch GT911...");
  // Rotación 0 para nueva placa con táctil invertido
  touch_available =
      touch_init(Hardware::SCREEN_WIDTH, Hardware::SCREEN_HEIGHT, 0);
  if (touch_available) {
    Serial.println("[Touch] ✓ OK\n");
  } else {
    Serial.println("[Touch] ⚠ Touch no disponible, continuando sin touch\n");
  }

  // Inicializar LVGL 9.x
  Serial.println("[4/7] Inicializando LVGL 9.x...");
  lv_init();
  lv_tick_set_cb(millis_cb);
  Serial.println("[LVGL] ✓ lv_init() completado");

  // Crear buffer de display (usar PSRAM)
  Serial.println("\n[5/7] Creando buffer de display...");
  uint32_t bufSize = Hardware::SCREEN_WIDTH * 40;
  Serial.printf("[LVGL] Tamaño buffer: %d pixels (%d bytes)\n", bufSize,
                bufSize * sizeof(lv_color_t));

  disp_draw_buf = (lv_color_t *)heap_caps_malloc(
      bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!disp_draw_buf) {
    Serial.println("[LVGL] PSRAM no disponible, usando RAM interna...");
    disp_draw_buf = (lv_color_t *)malloc(bufSize * sizeof(lv_color_t));
  }

  if (!disp_draw_buf) {
    Serial.println("\n!!! ERROR FATAL: No hay memoria para LVGL buffer !!!");
    while (1)
      delay(1000);
  }

  Serial.println("[LVGL] ✓ Buffer creado en PSRAM");

  // Inicializar driver de display (LVGL 9)
  Serial.println("\n[LVGL] Inicializando driver de display...");
  disp = lv_display_create(Hardware::SCREEN_WIDTH, Hardware::SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL,
                         bufSize * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  Serial.println("[LVGL] ✓ Display driver registrado");

  // Inicializar driver de entrada (LVGL 9)
  Serial.println("[LVGL] Inicializando driver de touch...");
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  Serial.println("[LVGL] ✓ Touch driver registrado\n");

  // Inicializar UI
  Serial.println("[6/7] Inicializando interfaz de usuario...");
  UI::init_styles();
  Serial.println("[UI] ✓ Estilos inicializados");

  UI::Numpad::create();
  Serial.println("[UI] ✓ Numpad creado");

  UIScreens::init_all_screens();
  Serial.println("[UI] ✓ Pantallas creadas");

  if (UIScreens::screen_main) {
    lv_scr_load(UIScreens::screen_main);
    Serial.println("[UI] ✓ Pantalla principal cargada\n");
  }

  // Homing inicial
  Serial.println("[7/7] Ejecutando homing inicial...");
  UIHandlers::homing_ejes();

  // Timer para actualizar pantalla de bobinado
  lv_timer_create(
      [](lv_timer_t *timer) { UIHandlers::update_winding_screen(); }, 100,
      NULL);
  Serial.println("[LVGL] ✓ Timer de actualización creado");

  // Crear tareas FreeRTOS
  Serial.println("\n[FreeRTOS] Creando tareas...");

  // Tarea de Motores en Core 1 (Timing Crítico)
  xTaskCreatePinnedToCore(motor_control_task_optimized, "MotorCore1",
                          Hardware::Memory::TASK_STACK_SIZE, NULL,
                          configMAX_PRIORITIES - 1, NULL,
                          1 // Core 1
  );
  Serial.println("[FreeRTOS] ✓ Tarea de Motores (Core 1, Prioridad máxima)");

  // Tarea de Logging en Core 0 (Prioridad baja)
  xTaskCreatePinnedToCore(logging_task, "LoggingCore0", 4096, NULL,
                          1, // Prioridad baja
                          NULL,
                          0 // Core 0
  );
  Serial.println("[FreeRTOS] ✓ Tarea de Logging (Core 0, Prioridad baja)");

  // Tarea de UI (LVGL) en Core 0 (Prioridad media)
  xTaskCreatePinnedToCore(
      ui_task, "UICore0",
      8192, // Stack size generoso para LVGL
      NULL,
      2, // Prioridad media (mayor que logging, menor que motores)
      NULL,
      0 // Core 0
  );
  Serial.println("[FreeRTOS] ✓ Tarea de UI (Core 0, Prioridad media)");

  Serial.println("\n========================================");
  Serial.println("   ✓ SISTEMA INICIADO CORRECTAMENTE (LVGL 9)");
  Serial.println("========================================");
  Serial.println("Core 0: UI + Logging + Serial");
  Serial.println("Core 1: Control de motores (timing crítico)");
  Serial.println("========================================\n");

  Sistema::estado.imprimir_estado();

  // MARCAR LVGL COMO LISTO
  lvgl_ready = true;
  Serial.println("\n>>> Sistema listo para operar <<<\n");
}

// =========================================================================
// LOOP (NO USADO - UI MOVIDA A TAREA DEDICADA)
// =========================================================================
void loop() {
  // El loop de Arduino puede correr en Core 1 por defecto, lo que causa
  // conflictos con la tarea de motores. Por eso movimos la UI a ui_task en Core
  // 0. Aquí solo dormimos para no consumir CPU.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
