#ifndef WINDING_TRANSFORMER_CORE_H
#define WINDING_TRANSFORMER_CORE_H

#include <cmath>
#include <cstdint>

namespace winding {

struct MechanicalConfig {
  float pasos_por_mm_x = 320.0f;
  std::uint32_t pasos_por_vuelta_y = 3200;
  float min_rpm = 1.0f;
  float max_rpm = 500.0f;
};

struct TransformerConfig {
  float diametro_alambre_mm = 0.5f;
  float longitud_bobinado_mm = 50.0f;
  std::uint32_t vueltas_total = 1000;
  float velocidad_rpm = 100.0f;
  bool detener_en_capas = false;
};

struct TransformerDerived {
  std::uint32_t vueltas_por_capa = 0;
  std::uint32_t capas_estimadas = 0;
  float grosor_bobinado_mm = 0.0f;
  float step_ratio_x_per_y = 0.0f;
  std::int32_t limite_pasos_x = 0;
};

struct HoneycombConfig {
  float diametro_hilo = 0.2f;
  float diametro_carrete = 20.0f;
  float ancho_carrete = 10.0f;
  float desfase_grados = 72.0f;
  std::uint32_t num_vueltas = 500;
  float velocidad_rpm = 100.0f;
};

struct HoneycombDerived {
  float desfase_grados = 72.0f;
  float factor_desfase = 1.2f;
  std::uint32_t vueltas_por_capa = 0;
  std::uint32_t capas_estimadas = 0;
  float grosor_estimado = 0.0f;
  float step_ratio_x_per_y = 0.0f;
  std::int32_t pasos_x_recorrido_completo = 0;
  std::int32_t pasos_y_por_ciclo = 0;
};

struct ValidationResult {
  bool ok = true;
  const char *message = "OK";
};

struct TransformerState {
  std::uint32_t vueltas_completadas = 0;
  std::uint32_t vueltas_capa_actual = 0;
  std::uint32_t capas_completadas = 0;
  std::int8_t sentido_x = 1;
  std::int32_t pasos_x_acumulados = 0;
  bool pausado_por_capa = false;
  bool completado = false;
};

struct TransformerStepEvent {
  std::int32_t pasos_x = 0;
  bool vuelta_completada = false;
  bool capa_completada = false;
  bool pausa_solicitada = false;
  bool bobinado_completado = false;
};

inline TransformerDerived
calculate_transformer(const TransformerConfig &config,
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

inline HoneycombDerived calculate_honeycomb(const HoneycombConfig &config,
                                            const MechanicalConfig &mechanics) {
  HoneycombDerived derived;
  derived.desfase_grados = config.desfase_grados;
  derived.factor_desfase = 1.0f + (derived.desfase_grados / 360.0f);

  if (derived.factor_desfase < 0.1f) {
    derived.factor_desfase = 0.1f;
    derived.desfase_grados = (derived.factor_desfase - 1.0f) * 360.0f;
  }

  derived.pasos_x_recorrido_completo = static_cast<std::int32_t>(
      config.ancho_carrete * mechanics.pasos_por_mm_x * 2);

  derived.pasos_y_por_ciclo = static_cast<std::int32_t>(
      mechanics.pasos_por_vuelta_y * derived.factor_desfase);

  if (derived.pasos_y_por_ciclo > 0) {
    derived.step_ratio_x_per_y =
        static_cast<float>(derived.pasos_x_recorrido_completo) /
        derived.pasos_y_por_ciclo;
  }

  if (std::fabs(derived.desfase_grados) > 0.1f) {
    derived.vueltas_por_capa =
        static_cast<std::uint32_t>(360.0f /
                                   std::fabs(derived.desfase_grados));
  } else {
    derived.vueltas_por_capa = config.num_vueltas;
  }

  if (derived.vueltas_por_capa > 0) {
    derived.capas_estimadas = static_cast<std::uint32_t>(std::ceil(
        static_cast<float>(config.num_vueltas) / derived.vueltas_por_capa));
  } else {
    derived.capas_estimadas = 1;
  }

  if (derived.capas_estimadas < 1) {
    derived.capas_estimadas = 1;
  }

  derived.grosor_estimado =
      static_cast<float>(derived.capas_estimadas) * config.diametro_hilo;

  return derived;
}

ValidationResult validate_transformer(const TransformerConfig &config,
                                      const MechanicalConfig &mechanics);

class TransformerEngine {
public:
  TransformerEngine(const TransformerConfig &config,
                    const MechanicalConfig &mechanics = MechanicalConfig());

  const TransformerConfig &config() const;
  const MechanicalConfig &mechanics() const;
  const TransformerDerived &derived() const;
  const TransformerState &state() const;

  void reset();
  TransformerStepEvent step_y();

private:
  TransformerConfig config_;
  MechanicalConfig mechanics_;
  TransformerDerived derived_;
  TransformerState state_;
  float x_step_accumulator_ = 0.0f;
  std::uint32_t y_steps_current_turn_ = 0;
};

} // namespace winding

#endif // WINDING_TRANSFORMER_CORE_H
