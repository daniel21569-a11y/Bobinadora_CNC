#include "firmware_update.h"

#include "config.h"
#include "profile_manager.h"

#include <Update.h>
#include <mbedtls/sha256.h>

namespace FirmwareUpdate {
namespace {

constexpr const char *ROOT = "/firmware";
constexpr const char *BOARD = "JC4827W543";

String json_string(const String &json, const char *key) {
  String token = String("\"") + key + "\"";
  int start = json.indexOf(token);
  if (start < 0)
    return String();
  start = json.indexOf(':', start + token.length());
  if (start < 0)
    return String();
  start = json.indexOf('"', start + 1);
  if (start < 0)
    return String();
  int end = json.indexOf('"', start + 1);
  if (end < 0)
    return String();
  return json.substring(start + 1, end);
}

uint32_t json_uint(const String &json, const char *key) {
  String token = String("\"") + key + "\"";
  int start = json.indexOf(token);
  if (start < 0)
    return 0;
  start = json.indexOf(':', start + token.length());
  if (start < 0)
    return 0;
  while (start + 1 < (int)json.length() && json[start + 1] == ' ')
    ++start;
  return (uint32_t)json.substring(start + 1).toInt();
}

bool read_manifest(const String &path, Package &package) {
  File manifest = SD.open(path, FILE_READ);
  if (!manifest || manifest.size() > 4096)
    return false;

  String json;
  while (manifest.available())
    json += (char)manifest.read();
  manifest.close();

  package.version = json_string(json, "version");
  package.board = json_string(json, "board");
  package.sha256 = json_string(json, "sha256");
  String firmware = json_string(json, "firmware");
  uint32_t declared_size = json_uint(json, "size");

  if (package.version.isEmpty() || firmware.isEmpty() || package.board.isEmpty() ||
      package.sha256.length() != 64 || declared_size == 0)
    return false;
  if (firmware.indexOf('/') >= 0 || firmware.indexOf('\\') >= 0)
    return false;

  package.firmware_path = package.directory + "/" + firmware;
  File image = SD.open(package.firmware_path, FILE_READ);
  if (!image)
    return false;
  package.size = image.size();
  image.close();

  package.compatible = package.board == BOARD &&
                       (declared_size == 0 || declared_size == package.size);
  return true;
}

bool calculate_sha256(File &image, String &result,
                      void (*progress)(uint8_t)) {
  if (!image)
    return false;

  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  mbedtls_sha256_starts(&context, 0);

  if (!image.seek(0)) {
    mbedtls_sha256_free(&context);
    return false;
  }

  uint8_t buffer[4096];
  size_t total = image.size();
  size_t processed = 0;
  while (image.available()) {
    size_t count = image.read(buffer, sizeof(buffer));
    if (count == 0) {
      mbedtls_sha256_free(&context);
      return false;
    }
    mbedtls_sha256_update(&context, buffer, count);
    processed += count;
    if (progress && total)
      progress((uint8_t)((processed * 50) / total));
    delay(1);
  }

  if (processed != total) {
    mbedtls_sha256_free(&context);
    return false;
  }

  uint8_t digest[32];
  mbedtls_sha256_finish(&context, digest);
  bool ok = true;
  mbedtls_sha256_free(&context);
  if (!ok)
    return false;

  result = "";
  const char hex[] = "0123456789abcdef";
  for (uint8_t value : digest) {
    result += hex[value >> 4];
    result += hex[value & 0x0F];
  }
  Serial.printf("[FirmwareUpdate] SHA SD: %s\\n", result.c_str());
  return true;
}

} // namespace

bool scan_sd(std::vector<Package> &packages, String &error) {
  packages.clear();
  if (!ProfileManager::init()) {
    error = "No se pudo inicializar la tarjeta SD";
    return false;
  }
  if (!SD.exists(ROOT)) {
    error = "No existe la carpeta /firmware";
    return false;
  }

  File root = SD.open(ROOT, FILE_READ);
  if (!root || !root.isDirectory()) {
    error = "La carpeta /firmware no es valida";
    return false;
  }

  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      Package package;
      String entry_name = entry.name();
      package.directory = entry_name.startsWith(ROOT)
                              ? entry_name
                              : String(ROOT) + "/" + entry_name;
      if (read_manifest(package.directory + "/manifest.json", package))
        packages.push_back(package);
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();

  if (packages.empty()) {
    error = "No se encontraron versiones validas";
    return false;
  }
  return true;
}

bool install_from_sd(const Package &package, String &error,
                     void (*progress)(uint8_t)) {
  if (Sistema::estado.estado != EstadoBobinado::LISTO) {
    error = "La maquina debe estar en estado LISTO";
    return false;
  }
  if (!package.compatible) {
    error = "El firmware no es compatible con esta placa";
    return false;
  }
  if (package.version == FirmwareInfo::VERSION) {
    error = "La version seleccionada ya esta instalada";
    return false;
  }

  File image = SD.open(package.firmware_path, FILE_READ);
  if (!image || image.size() != package.size) {
    if (image)
      image.close();
    error = "No se puede abrir el firmware";
    return false;
  }

  String calculated;
  if (!calculate_sha256(image, calculated, progress) ||
      !calculated.equalsIgnoreCase(package.sha256)) {
    Serial.printf("[FirmwareUpdate] SHA esperado: %s\\n",
                  package.sha256.c_str());
    error = "Hash distinto\\nLeido: " + calculated.substring(0, 16) +
            "\\nEsperado: " + package.sha256.substring(0, 16);
    return false;
  }

  if (!Update.begin(package.size)) {
    error = "No hay particion OTA disponible";
    return false;
  }

  if (!image.seek(0)) {
    Update.abort();
    error = "No se puede reposicionar el firmware";
    return false;
  }

  size_t written = 0;
  uint8_t buffer[4096];
  while (image.available()) {
    size_t count = image.read(buffer, sizeof(buffer));
    if (count == 0 || Update.write(buffer, count) != count) {
      Update.abort();
      image.close();
      error = "Error escribiendo la particion OTA";
      return false;
    }
    written += count;
    if (progress && package.size)
      progress(50 + (uint8_t)((written * 50) / package.size));
    delay(1);
  }
  image.close();

  if (!Update.end(true) || !Update.isFinished()) {
    error = "No se pudo finalizar la actualizacion";
    return false;
  }
  if (progress)
    progress(100);
  return true;
}

} // namespace FirmwareUpdate
