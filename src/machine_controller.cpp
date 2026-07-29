#include "machine_controller.h"

namespace MachineController {

bool dispatch(winding::MachineCommand command) {
  switch (command) {
  case winding::MachineCommand::Start:
    if (Sistema::estado.estado != EstadoBobinado::LISTO ||
        !Sistema::estado.homing_completado) {
      return false;
    }
    Sistema::estado.iniciar_bobinado();
    return true;

  case winding::MachineCommand::Pause:
    if (Sistema::estado.estado != EstadoBobinado::BOBINANDO) {
      return false;
    }
    Sistema::estado.pausar_bobinado();
    return true;

  case winding::MachineCommand::Resume:
    if (Sistema::estado.estado != EstadoBobinado::PAUSADO) {
      return false;
    }
    Sistema::estado.reanudar_bobinado();
    return true;

  case winding::MachineCommand::Stop:
    Sistema::estado.detener_bobinado();
    Sistema::estado.reset_solicitado = true;
    return true;

  case winding::MachineCommand::Home:
    return Sistema::estado.estado == EstadoBobinado::LISTO ||
           Sistema::estado.estado == EstadoBobinado::PAUSADO ||
           Sistema::estado.estado == EstadoBobinado::ERROR;

  case winding::MachineCommand::Reset:
    Sistema::estado.reset();
    Sistema::estado.reset_solicitado = true;
    return true;

  case winding::MachineCommand::ManualXPositive:
  case winding::MachineCommand::ManualXNegative:
  case winding::MachineCommand::ManualYPositive:
  case winding::MachineCommand::ManualYNegative:
    return Sistema::estado.puede_mover_manual();

  case winding::MachineCommand::None:
    return false;
  }

  return false;
}

void on_homing_finished(bool success) {
  Sistema::estado.homing_completado = success;
  if (success) {
    Sistema::estado.estado = EstadoBobinado::LISTO;
    Sistema::estado.movimiento_manual_activo = false;
  } else {
    Sistema::estado.estado = EstadoBobinado::ERROR;
    Sistema::estado.movimiento_manual_activo = false;
    Sistema::estado.rpm_objetivo = 0.0f;
  }
}

void on_motion_error() {
  Sistema::estado.homing_completado = false;
  Sistema::estado.estado = EstadoBobinado::ERROR;
  Sistema::estado.rpm_objetivo = 0.0f;
  Sistema::estado.reset_solicitado = true;
}

void on_winding_completed() {
  Sistema::estado.estado = EstadoBobinado::LISTO;
  Sistema::estado.bobinado_completado = true;
  Sistema::estado.reset_solicitado = true;
}

} // namespace MachineController
