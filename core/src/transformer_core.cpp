#include "winding/transformer_core.h"

#include <cmath>

namespace winding {

TransformerDerived calculate_transformer(const TransformerConfig &config,
                                         const MechanicalConfig &mechanics) {
  TransformerDerived derived;

  if (config.diametro_alambre_mm > 0.0f) {
    derived.vueltas_por_capa = static_cast<std::uint32_t>(
        std::floor(config.longitud_bobinado_mm / config.diametro_alambre_mm));
  }

  if (mechanics.pasos_por_vuelta_y > 0) {
    const float pasos_x_por_vuelta =
        config.diametro_alambre_mm * mechanics.pasos_por_mm_x;
    derived.step_ratio_x_per_y =
        pasos_x_por_vuelta /
        static_cast<float>(mechanics.pasos_por_vuelta_y);
  }

  derived.limite_pasos_x = static_cast<std::int32_t>(
      std::round(config.longitud_bobinado_mm * mechanics.pasos_por_mm_x));

  if (derived.vueltas_por_capa > 0) {
    derived.capas_estimadas = static_cast<std::uint32_t>(std::ceil(
        static_cast<float>(config.vueltas_total) / derived.vueltas_por_capa));
    if (derived.capas_estimadas == 0 && config.vueltas_total > 0) {
      derived.capas_estimadas = 1;
    }
  }

  derived.grosor_bobinado_mm =
      static_cast<float>(derived.capas_estimadas) *
      config.diametro_alambre_mm;

  return derived;
}

ValidationResult validate_transformer(const TransformerConfig &config,
                                      const MechanicalConfig &mechanics) {
  if (config.diametro_alambre_mm <= 0.0f ||
      config.diametro_alambre_mm > 10.0f) {
    return {false, "diametro_alambre_mm fuera de rango"};
  }

  if (config.longitud_bobinado_mm <= 0.0f ||
      config.longitud_bobinado_mm > 500.0f) {
    return {false, "longitud_bobinado_mm fuera de rango"};
  }

  if (config.vueltas_total == 0 || config.vueltas_total > 100000) {
    return {false, "vueltas_total fuera de rango"};
  }

  if (config.velocidad_rpm < mechanics.min_rpm ||
      config.velocidad_rpm > mechanics.max_rpm) {
    return {false, "velocidad_rpm fuera de rango"};
  }

  if (mechanics.pasos_por_mm_x <= 0.0f ||
      mechanics.pasos_por_vuelta_y == 0) {
    return {false, "configuracion mecanica invalida"};
  }

  return {};
}

TransformerEngine::TransformerEngine(const TransformerConfig &config,
                                     const MechanicalConfig &mechanics)
    : config_(config), mechanics_(mechanics),
      derived_(calculate_transformer(config_, mechanics_)) {}

const TransformerConfig &TransformerEngine::config() const { return config_; }

const MechanicalConfig &TransformerEngine::mechanics() const {
  return mechanics_;
}

const TransformerDerived &TransformerEngine::derived() const {
  return derived_;
}

const TransformerState &TransformerEngine::state() const { return state_; }

void TransformerEngine::reset() {
  state_ = {};
  x_step_accumulator_ = 0.0f;
  y_steps_current_turn_ = 0;
}

TransformerStepEvent TransformerEngine::step_y() {
  TransformerStepEvent event;

  if (state_.completado || state_.pausado_por_capa) {
    return event;
  }

  if (derived_.limite_pasos_x > 0 && derived_.step_ratio_x_per_y > 0.0f) {
    x_step_accumulator_ += derived_.step_ratio_x_per_y;

    while (x_step_accumulator_ >= 1.0f) {
      const std::int32_t step_x = state_.sentido_x;
      state_.pasos_x_acumulados += step_x;
      event.pasos_x += step_x;
      x_step_accumulator_ -= 1.0f;

      if (state_.pasos_x_acumulados >= derived_.limite_pasos_x ||
          state_.pasos_x_acumulados <= 0) {
        state_.pasos_x_acumulados =
            (state_.pasos_x_acumulados >= derived_.limite_pasos_x)
                ? derived_.limite_pasos_x
                : 0;
        state_.capas_completadas++;
        state_.vueltas_capa_actual = 0;
        state_.sentido_x = static_cast<std::int8_t>(state_.sentido_x * -1);
        event.capa_completada = true;

        if (config_.detener_en_capas &&
            state_.vueltas_completadas < config_.vueltas_total) {
          state_.pausado_por_capa = true;
          event.pausa_solicitada = true;
        }
        break;
      }
    }
  }

  y_steps_current_turn_++;
  if (y_steps_current_turn_ >= mechanics_.pasos_por_vuelta_y) {
    y_steps_current_turn_ = 0;
    state_.vueltas_completadas++;
    state_.vueltas_capa_actual++;
    event.vuelta_completada = true;
  }

  if (state_.vueltas_completadas >= config_.vueltas_total) {
    state_.completado = true;
    event.bobinado_completado = true;
  }

  return event;
}

} // namespace winding
