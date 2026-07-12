#ifndef WINDING_MACHINE_STATE_H
#define WINDING_MACHINE_STATE_H

#include <cstdint>

namespace winding {

enum class MachineStatus : std::uint8_t {
  Ready,
  Winding,
  Paused,
  Error,
  Homing,
  Manual,
  Complete
};

enum class WindingMode : std::uint8_t {
  Transformer,
  Honeycomb
};

enum class MachineCommand : std::uint8_t {
  None,
  Start,
  Pause,
  Resume,
  Stop,
  Reset,
  Home,
  ManualXPositive,
  ManualXNegative,
  ManualYPositive,
  ManualYNegative
};

enum class MachineError : std::uint8_t {
  None,
  InvalidConfig,
  LimitSwitchX,
  LimitSwitchY,
  HomingTimeout,
  HomingFailed,
  MotionFault,
  StorageFault,
  Unknown
};

struct MachineSnapshot {
  MachineStatus status = MachineStatus::Ready;
  WindingMode mode = WindingMode::Transformer;
  MachineError error = MachineError::None;

  std::uint32_t vueltas_completadas = 0;
  std::uint32_t vueltas_capa_actual = 0;
  std::uint32_t capas_completadas = 0;

  std::int8_t sentido_x = 1;
  std::int32_t pasos_x_acumulados = 0;

  float rpm_actual = 0.0f;
  float rpm_objetivo = 0.0f;

  bool reset_solicitado = false;
  bool movimiento_manual_activo = false;
  bool mantener_motor_en_pausa = true;
  bool bobinado_completado = false;
};

} // namespace winding

#endif // WINDING_MACHINE_STATE_H
