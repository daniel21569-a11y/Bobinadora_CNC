#ifndef WINDING_CORE_ADAPTERS_H
#define WINDING_CORE_ADAPTERS_H

#include "config.h"
#include "../core/include/winding/machine_state.h"

namespace WindingCoreAdapters {

inline winding::MachineStatus toMachineStatus(EstadoBobinado estado) {
  switch (estado) {
  case EstadoBobinado::LISTO:
    return winding::MachineStatus::Ready;
  case EstadoBobinado::BOBINANDO:
    return winding::MachineStatus::Winding;
  case EstadoBobinado::PAUSADO:
    return winding::MachineStatus::Paused;
  case EstadoBobinado::ERROR:
    return winding::MachineStatus::Error;
  case EstadoBobinado::HOMING:
    return winding::MachineStatus::Homing;
  case EstadoBobinado::MANUAL:
    return winding::MachineStatus::Manual;
  }

  return winding::MachineStatus::Error;
}

inline winding::WindingMode toWindingMode(ModoBobinado modo) {
  switch (modo) {
  case ModoBobinado::TRANSFORMADOR:
    return winding::WindingMode::Transformer;
  case ModoBobinado::NIDO_ABEJA:
    return winding::WindingMode::Honeycomb;
  }

  return winding::WindingMode::Transformer;
}

inline winding::MachineCommand toMachineCommand(TipoMovimiento movimiento) {
  switch (movimiento) {
  case TipoMovimiento::NINGUNO:
    return winding::MachineCommand::None;
  case TipoMovimiento::MANUAL_X_POS:
    return winding::MachineCommand::ManualXPositive;
  case TipoMovimiento::MANUAL_X_NEG:
    return winding::MachineCommand::ManualXNegative;
  case TipoMovimiento::MANUAL_Y_POS:
    return winding::MachineCommand::ManualYPositive;
  case TipoMovimiento::MANUAL_Y_NEG:
    return winding::MachineCommand::ManualYNegative;
  case TipoMovimiento::HOMING_X:
  case TipoMovimiento::HOMING_Y:
    return winding::MachineCommand::Home;
  }

  return winding::MachineCommand::None;
}

inline winding::MachineSnapshot toMachineSnapshot(const EstadoSistema &estado) {
  winding::MachineSnapshot snapshot;

  snapshot.status = toMachineStatus(estado.estado);
  snapshot.mode = toWindingMode(estado.modo);
  snapshot.error = (estado.estado == EstadoBobinado::ERROR)
                       ? winding::MachineError::Unknown
                       : winding::MachineError::None;

  snapshot.vueltas_completadas = estado.vueltas_completadas;
  snapshot.vueltas_capa_actual = estado.vueltas_capa_actual;
  snapshot.capas_completadas = estado.capas_completadas;
  snapshot.sentido_x = estado.sentido_X;
  snapshot.pasos_x_acumulados = estado.pasos_X_acumulados;
  snapshot.rpm_actual = estado.rpm_actual;
  snapshot.rpm_objetivo = estado.rpm_objetivo;
  snapshot.reset_solicitado = estado.reset_solicitado;
  snapshot.movimiento_manual_activo = estado.movimiento_manual_activo;
  snapshot.mantener_motor_en_pausa = estado.mantener_motor_en_pausa;
  snapshot.bobinado_completado = estado.bobinado_completado;

  return snapshot;
}

} // namespace WindingCoreAdapters

#endif // WINDING_CORE_ADAPTERS_H
