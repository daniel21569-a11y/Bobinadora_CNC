// =========================================================================
// TAREA DE CONTROL DE MOTORES OPTIMIZADA - CORE 1
// VERSIÓN: 10.3 - DIR EN CADA PASO (COMO TRANSFORMADOR)
// =========================================================================
// CAMBIOS CLAVE:
// - Core 1: SOLO genera pulsos STEP/DIR (timing crítico)
// - Honeycomb: Dirección se establece EN CADA PASO (como transformador)
// - Ratio pre-calculado (sin divisiones en loop)
// =========================================================================

#ifndef MOTOR_TASK_OPTIMIZED_H
#define MOTOR_TASK_OPTIMIZED_H

#include "config.h"
#include "machine_controller.h"

// =========================================================================
// ESTRUCTURA DE ESTADO LOCAL DEL MOTOR (SOLO CORE 1)
// =========================================================================
struct MotorStateCore1 {
  // Timing de pulsos (variables críticas)
  unsigned long last_step_time_Y;
  unsigned long last_speed_update_us;
  float step_period_us_Y_current;

  // Acumuladores de pasos
  float X_step_accumulator_transformer;
  float X_step_accumulator_honeycomb;

  // Estado de nido de abeja
  unsigned long pasos_Y_en_ciclo_actual;
  long pasos_Y_por_ciclo_honeycomb;
  long pasos_X_recorrido_ida;
  long mitad_ciclo_Y;
  bool direccion_X_honeycomb;
  long posicion_X_inicio_ciclo;
  float step_ratio_X_per_Y_honeycomb;

  // Contadores
  unsigned long current_Y_steps;

  // Estado previo
  EstadoBobinado ultimo_estado;

  // Flags de inicialización
  bool ciclo_inicializado;

  void reset() {
    last_step_time_Y = micros();
    last_speed_update_us = last_step_time_Y;
    step_period_us_Y_current = 0.0f;

    X_step_accumulator_transformer = 0.0f;
    X_step_accumulator_honeycomb = 0.0f;

    pasos_Y_en_ciclo_actual = 0;
    pasos_Y_por_ciclo_honeycomb = 0;
    pasos_X_recorrido_ida = 0;
    mitad_ciclo_Y = 0;
    direccion_X_honeycomb = true;
    posicion_X_inicio_ciclo = 0;
    step_ratio_X_per_Y_honeycomb = 0.0f;

    current_Y_steps = 0;
    ultimo_estado = EstadoBobinado::LISTO;
    ciclo_inicializado = false;
  }
};

// Variable local del Core 1 (no compartida)
static MotorStateCore1 motor_state_c1;

// =========================================================================
// FUNCIONES INLINE PARA MÁXIMA VELOCIDAD
// =========================================================================

inline void pulse_step_pin(int pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(Hardware::Motor::STEP_PULSE_US);
  digitalWrite(pin, LOW);
}

inline void set_dir_x_fast(bool forward) {
  digitalWrite(Hardware::Motor::DIR_X_PIN, forward ? HIGH : LOW);
}

inline void set_dir_y_fast(bool forward) {
  digitalWrite(Hardware::Motor::DIR_Y_PIN, forward ? HIGH : LOW);
}

inline void step_x_multiple(long num_steps, bool forward) {
  if (num_steps == 0)
    return;

  set_dir_x_fast(forward);
  delayMicroseconds(5);

  for (long i = 0; i < num_steps; i++) {
    pulse_step_pin(Hardware::Motor::STEP_X_PIN);
    delayMicroseconds(Hardware::Motor::STEP_PULSE_US);
  }
}

// =========================================================================
// FUNCIÓN DE ACTUALIZACIÓN DE VELOCIDAD CON RAMPA
// =========================================================================
inline void update_speed_ramp(unsigned long now) {
  float dt_s = (now - motor_state_c1.last_speed_update_us) / 1000000.0f;
  motor_state_c1.last_speed_update_us = now;

  if (Sistema::estado.estado == EstadoBobinado::BOBINANDO) {
    Sistema::estado.rpm_objetivo =
        (Sistema::estado.modo == ModoBobinado::TRANSFORMADOR)
            ? Sistema::config_transformador.velocidad_rpm
            : Sistema::config_nido_abeja.velocidad_rpm;
  } else {
    Sistema::estado.rpm_objetivo = 0.0f;
  }

  if (Sistema::estado.rpm_actual < Sistema::estado.rpm_objetivo) {
    Sistema::estado.rpm_actual = fminf(
        Sistema::estado.rpm_objetivo,
        Sistema::estado.rpm_actual + Hardware::Motor::RPM_ACCELERATION * dt_s);
  } else if (Sistema::estado.rpm_actual > Sistema::estado.rpm_objetivo) {
    Sistema::estado.rpm_actual = fmaxf(
        Sistema::estado.rpm_objetivo,
        Sistema::estado.rpm_actual - Hardware::Motor::RPM_ACCELERATION * dt_s);
  }

  motor_state_c1.step_period_us_Y_current =
      (Sistema::estado.rpm_actual > 0.01f)
          ? 1000000.0f / ((Sistema::estado.rpm_actual *
                           Hardware::Motor::PASOS_POR_VUELTA_Y) /
                          60.0f)
          : 0.0f;
}

// =========================================================================
// BOBINADO MODO TRANSFORMADOR - OPTIMIZADO
// =========================================================================
inline void process_transformer_step() {
  long limite_pasos_X = Sistema::config_transformador.limite_pasos_X;

  if (limite_pasos_X > 0 &&
      Sistema::config_transformador.step_ratio_X_per_Y > 0.0f) {
    motor_state_c1.X_step_accumulator_transformer +=
        Sistema::config_transformador.step_ratio_X_per_Y;

    while (motor_state_c1.X_step_accumulator_transformer >= 1.0f) {
      set_dir_x_fast(Sistema::estado.sentido_X == 1);
      pulse_step_pin(Hardware::Motor::STEP_X_PIN);

      Sistema::estado.pasos_X_acumulados += Sistema::estado.sentido_X;
      motor_state_c1.X_step_accumulator_transformer -= 1.0f;

      if (Sistema::estado.pasos_X_acumulados >= limite_pasos_X ||
          Sistema::estado.pasos_X_acumulados <= 0) {

        Sistema::estado.pasos_X_acumulados =
            (Sistema::estado.pasos_X_acumulados >= limite_pasos_X)
                ? limite_pasos_X
                : 0;
        Sistema::estado.capas_completadas++;
        Sistema::estado.vueltas_capa_actual = 0;
        Sistema::estado.sentido_X *= -1;

        if (Sistema::config_transformador.detener_en_capas &&
            Sistema::estado.vueltas_completadas <
                Sistema::config_transformador.vueltas_total) {
          Sistema::estado.estado = EstadoBobinado::PAUSADO;
        }
        break;
      }
    }
  }

  motor_state_c1.current_Y_steps++;
  if (motor_state_c1.current_Y_steps >=
      (unsigned long)Hardware::Motor::PASOS_POR_VUELTA_Y) {
    motor_state_c1.current_Y_steps = 0;
    Sistema::estado.vueltas_completadas++;
    Sistema::estado.vueltas_capa_actual++;
  }
}

// =========================================================================
// BOBINADO MODO NIDO DE ABEJA - SINUSOIDAL PRECISO
// =========================================================================
inline void init_honeycomb_cycle() {
  // Calcular parámetros del ciclo
  motor_state_c1.pasos_Y_por_ciclo_honeycomb =
      (long)(Hardware::Motor::PASOS_POR_VUELTA_Y *
             Sistema::config_nido_abeja.factor_desfase);

  // Amplitud en pasos (mitad del ancho total)
  float amplitud_mm = Sistema::config_nido_abeja.ancho_carrete / 2.0f;
  motor_state_c1.pasos_X_recorrido_ida =
      (long)(amplitud_mm * Hardware::Motor::PASOS_POR_MM_X);

  motor_state_c1.pasos_Y_en_ciclo_actual = 0;
  motor_state_c1.X_step_accumulator_honeycomb = 0.0f;
  Sistema::estado.pasos_X_acumulados = 0; // Posición relativa al centro

  motor_state_c1.ciclo_inicializado = true;
}

inline void process_honeycomb_step() {
  if (!motor_state_c1.ciclo_inicializado) {
    init_honeycomb_cycle();
  }

  // 1. Calcular progreso del ciclo (0.0 a 2*PI)
  float progreso_ciclo = (float)motor_state_c1.pasos_Y_en_ciclo_actual /
                         (float)motor_state_c1.pasos_Y_por_ciclo_honeycomb;
  float angulo = progreso_ciclo * 2.0f * PI;

  // 2. Calcular posición X objetivo (Sinusoidal)
  // Usamos sinf() para movimiento suave. El rango es [-amplitud, +amplitud]
  float posicion_X_objetivo =
      sinf(angulo) * motor_state_c1.pasos_X_recorrido_ida;

  // 3. Calcular diferencia con posición actual
  float error_posicion =
      posicion_X_objetivo - Sistema::estado.pasos_X_acumulados;

  // 4. Mover motor X si el error es mayor a 1 paso
  if (abs(error_posicion) >= 1.0f) {
    bool direccion = (error_posicion > 0);
    set_dir_x_fast(direccion);
    pulse_step_pin(Hardware::Motor::STEP_X_PIN);

    if (direccion) {
      Sistema::estado.pasos_X_acumulados++;
    } else {
      Sistema::estado.pasos_X_acumulados--;
    }
  }

  // 5. Avanzar ciclo Y
  motor_state_c1.pasos_Y_en_ciclo_actual++;

  // 6. Verificar fin de ciclo
  if (motor_state_c1.pasos_Y_en_ciclo_actual >=
      motor_state_c1.pasos_Y_por_ciclo_honeycomb) {
    motor_state_c1.pasos_Y_en_ciclo_actual = 0;

    // Actualizar contadores de vueltas
    Sistema::estado.vueltas_completadas++;
    Sistema::estado.vueltas_capa_actual++;

    // Actualizar capa
    if (Sistema::estado.vueltas_capa_actual >=
        Sistema::config_nido_abeja.vueltas_por_capa) {
      Sistema::estado.capas_completadas++;
      Sistema::estado.vueltas_capa_actual = 0;
    }
  }
}

// =========================================================================
// TAREA PRINCIPAL DE CONTROL DE MOTORES - CORE 1
// =========================================================================
void motor_control_task_optimized(void *pvParameters) {
  motor_state_c1.reset();

  // Contador para yield periódico (evitar monopolizar CPU)
  static uint32_t loop_counter = 0;

  for (;;) {
    // Yield cada 1000 iteraciones para permitir que el watchdog y otras tareas
    // funcionen
    if (++loop_counter >= 1000) {
      loop_counter = 0;
      taskYIELD(); // Permitir que el scheduler ejecute otras tareas
    }
    if (Sistema::estado.estado == EstadoBobinado::BOBINANDO) {
      bool limite_x = !digitalRead(Hardware::Motor::LIMIT_X_PIN);
      if (limite_x) {
        digitalWrite(Hardware::Motor::ENABLE_X_PIN, HIGH);
        digitalWrite(Hardware::Motor::ENABLE_Y_PIN, HIGH);
        MachineController::on_motion_error();
        continue;
      }
    }

    if (Sistema::estado.reset_solicitado) {
      motor_state_c1.reset();
      digitalWrite(Hardware::Motor::ENABLE_X_PIN, HIGH);
      digitalWrite(Hardware::Motor::ENABLE_Y_PIN, HIGH);
      Sistema::estado.reset_solicitado = false;
      vTaskDelay(10);
      continue;
    }

    if (Sistema::estado.estado == EstadoBobinado::BOBINANDO &&
        motor_state_c1.ultimo_estado != EstadoBobinado::BOBINANDO) {
      motor_state_c1.reset();
      motor_state_c1.last_step_time_Y = micros();
      motor_state_c1.last_speed_update_us = micros();
    }
    motor_state_c1.ultimo_estado = Sistema::estado.estado;

    if (Sistema::estado.movimiento_manual_activo) {
      vTaskDelay(1);
      continue;
    }

    bool enable_motores =
        (Sistema::estado.estado == EstadoBobinado::BOBINANDO) ||
        (Sistema::estado.estado == EstadoBobinado::PAUSADO &&
         Sistema::estado.mantener_motor_en_pausa);
    digitalWrite(Hardware::Motor::ENABLE_X_PIN, !enable_motores);
    digitalWrite(Hardware::Motor::ENABLE_Y_PIN, !enable_motores);

    unsigned long now = micros();
    update_speed_ramp(now);

    if (Sistema::estado.estado == EstadoBobinado::BOBINANDO &&
        motor_state_c1.step_period_us_Y_current > 0.0f) {

      if ((now - motor_state_c1.last_step_time_Y) >=
          motor_state_c1.step_period_us_Y_current) {
        set_dir_y_fast(true);
        pulse_step_pin(Hardware::Motor::STEP_Y_PIN);
        motor_state_c1.last_step_time_Y = now;

        if (Sistema::estado.modo == ModoBobinado::TRANSFORMADOR) {
          process_transformer_step();
        } else {
          process_honeycomb_step();
        }

        uint32_t vueltas_objetivo =
            (Sistema::estado.modo == ModoBobinado::TRANSFORMADOR)
                ? Sistema::config_transformador.vueltas_total
                : Sistema::config_nido_abeja.num_vueltas;

        if (Sistema::estado.vueltas_completadas >= vueltas_objetivo) {
          MachineController::on_winding_completed();
        }
      }
    } else {
      vTaskDelay(1);
    }
  }
}

#endif // MOTOR_TASK_OPTIMIZED_H
