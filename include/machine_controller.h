#ifndef MACHINE_CONTROLLER_H
#define MACHINE_CONTROLLER_H

#include "config.h"
#include "../core/include/winding/machine_state.h"

namespace MachineController {

// Punto unico de entrada para las ordenes de alto nivel de la maquina.
bool dispatch(winding::MachineCommand command);

// Eventos producidos por hardware y por la tarea de motores.
void on_homing_finished(bool success);
void on_motion_error();
void on_winding_completed();

} // namespace MachineController

#endif // MACHINE_CONTROLLER_H
