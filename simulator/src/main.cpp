#include "winding/transformer_core.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace {

int fail(const char *message) {
  std::cerr << "ERROR: " << message << '\n';
  return EXIT_FAILURE;
}

} // namespace

int main() {
  const winding::MechanicalConfig mechanics;
  const winding::TransformerConfig config;

  const winding::ValidationResult validation =
      winding::validate_transformer(config, mechanics);
  if (!validation.ok) {
    return fail(validation.message);
  }

  winding::TransformerEngine engine(config, mechanics);
  const winding::TransformerDerived &derived = engine.derived();

  std::cout << "Bobinadora CNC v11 - simulador transformador\n";
  std::cout << "============================================\n";
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Diametro alambre: " << config.diametro_alambre_mm << " mm\n";
  std::cout << "Longitud bobinado: " << config.longitud_bobinado_mm << " mm\n";
  std::cout << "Vueltas objetivo: " << config.vueltas_total << '\n';
  std::cout << "Vueltas/capa: " << derived.vueltas_por_capa << '\n';
  std::cout << "Capas estimadas: " << derived.capas_estimadas << '\n';
  std::cout << "Limite X: " << derived.limite_pasos_x << " pasos\n";
  std::cout << "Ratio X/Y: " << derived.step_ratio_x_per_y << '\n';
  std::cout << '\n';

  std::uint64_t pasos_y = 0;
  std::uint32_t eventos_capa = 0;
  std::uint32_t eventos_vuelta = 0;

  while (!engine.state().completado && !engine.state().pausado_por_capa) {
    const winding::TransformerStepEvent event = engine.step_y();
    pasos_y++;

    if (event.vuelta_completada) {
      eventos_vuelta++;
    }

    if (event.capa_completada) {
      eventos_capa++;
      std::cout << "Capa completada: " << eventos_capa
                << " | vuelta: " << engine.state().vueltas_completadas
                << " | X: " << engine.state().pasos_x_acumulados
                << " | sentido: " << static_cast<int>(engine.state().sentido_x)
                << '\n';
    }
  }

  const winding::TransformerState &state = engine.state();

  std::cout << "\nResultado\n";
  std::cout << "---------\n";
  std::cout << "Pasos Y simulados: " << pasos_y << '\n';
  std::cout << "Vueltas completadas: " << state.vueltas_completadas << '\n';
  std::cout << "Vueltas de capa actual: " << state.vueltas_capa_actual << '\n';
  std::cout << "Capas completadas: " << state.capas_completadas << '\n';
  std::cout << "Eventos vuelta: " << eventos_vuelta << '\n';
  std::cout << "Eventos capa: " << eventos_capa << '\n';
  std::cout << "Posicion X final: " << state.pasos_x_acumulados << '\n';
  std::cout << "Completado: " << (state.completado ? "si" : "no") << '\n';

  if (!state.completado) {
    return fail("la simulacion no llego a completarse");
  }

  if (state.vueltas_completadas != config.vueltas_total) {
    return fail("vueltas completadas no coincide con objetivo");
  }

  if (eventos_vuelta != config.vueltas_total) {
    return fail("eventos de vuelta no coincide con objetivo");
  }

  if (pasos_y != static_cast<std::uint64_t>(config.vueltas_total) *
                     mechanics.pasos_por_vuelta_y) {
    return fail("pasos Y simulados no coincide con vueltas * pasos/vuelta");
  }

  std::cout << "\nOK: simulacion transformador coherente.\n";
  return EXIT_SUCCESS;
}
