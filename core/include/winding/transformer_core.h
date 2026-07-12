#ifndef WINDING_TRANSFORMER_CORE_H
#define WINDING_TRANSFORMER_CORE_H

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

TransformerDerived calculate_transformer(const TransformerConfig &config,
                                         const MechanicalConfig &mechanics);

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
