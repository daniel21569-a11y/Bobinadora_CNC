#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <Arduino.h>
#include <vector>

namespace FirmwareUpdate {

struct Package {
  String version;
  String directory;
  String firmware_path;
  String board;
  String sha256;
  size_t size = 0;
  bool compatible = false;
};

bool scan_sd(std::vector<Package> &packages, String &error);
bool install_from_sd(const Package &package, String &error,
                     void (*progress)(uint8_t) = nullptr);

} // namespace FirmwareUpdate

#endif
