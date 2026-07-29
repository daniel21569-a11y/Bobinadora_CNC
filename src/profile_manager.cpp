#include "profile_manager.h"
#include <Arduino.h>

namespace ProfileManager {

static bool sd_initialized = false;
static constexpr const char *SD_MOUNT_POINT = "/sd";
static SPIClass spiSD(HSPI); // Instancia estática de SPI para SD

static void run_sd_diagnostic_if_requested() {
  constexpr const char *FLAG = "/sd_diagnostic.flag";
  constexpr const char *DATA = "/sd_diagnostic.bin";
  if (!SD.exists(FLAG))
    return;

  Serial.println("[SD-DIAG] Marcador detectado; iniciando prueba aislada");
  File file = SD.open(DATA, FILE_READ);
  if (!file) {
    Serial.println("[SD-DIAG] ERROR: no se pudo abrir sd_diagnostic.bin");
    SD.remove(FLAG);
    return;
  }

  const size_t expected = file.size();
  uint8_t buffer[512];
  bool ok = expected > 0;
  for (uint8_t pass = 1; pass <= 10 && ok; ++pass) {
    if (!file.seek(0)) {
      ok = false;
      break;
    }
    size_t total = 0;
    while (file.available()) {
      size_t count = file.read(buffer, sizeof(buffer));
      if (count == 0) {
        ok = false;
        break;
      }
      total += count;
      delay(1);
    }
    Serial.printf("[SD-DIAG] Lectura %u/10: %u/%u bytes %s\n", pass,
                  (unsigned)total, (unsigned)expected,
                  (ok && total == expected) ? "OK" : "ERROR");
    ok = ok && total == expected;
  }
  file.close();
  SD.remove(FLAG);
  Serial.printf("[SD-DIAG] RESULTADO: %s\n", ok ? "SD ESTABLE" : "SD INESTABLE");
}

bool init() {
  if (sd_initialized) {
    return true;
  }

  Serial.println("[ProfileManager] Inicializando SD...");

  // Delay inicial para estabilización
  delay(100);

  // Configurar SPI para SD (igual que en el ejemplo AnimatedGIF)
  pinMode(Hardware::SD::CS_PIN, OUTPUT);
  digitalWrite(Hardware::SD::CS_PIN, HIGH);
  spiSD.begin(Hardware::SD::SCK_PIN, Hardware::SD::MISO_PIN,
              Hardware::SD::MOSI_PIN, Hardware::SD::CS_PIN);

  delay(50);

  // Velocidad conservadora para mejorar la compatibilidad con adaptadores SD.
  Serial.println("[ProfileManager] Intentando montar SD...");
  if (!SD.begin(Hardware::SD::CS_PIN, spiSD, 10000000, SD_MOUNT_POINT)) {
    Serial.println("[ProfileManager] ERROR: No se pudo inicializar SD");
    Serial.println("[ProfileManager] Verifica:");
    Serial.println("  - Que haya una tarjeta SD insertada");
    Serial.println("  - Que la tarjeta esté formateada en FAT32");
    Serial.println("  - Pines: CS=10, MOSI=11, SCK=12, MISO=13");
    return false;
  }

  delay(50);

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[ProfileManager] ERROR: No hay tarjeta SD insertada");
    return false;
  }

  Serial.print("[ProfileManager] Tipo de tarjeta: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("[ProfileManager] Tamaño: %lluMB\n", cardSize);

  uint64_t cardUsed = SD.usedBytes() / (1024 * 1024);
  Serial.printf("[ProfileManager] Usado: %lluMB\n", cardUsed);

  sd_initialized = true;
  run_sd_diagnostic_if_requested();
  createDirectories();

  return true;
}

bool isSDAvailable() { return sd_initialized; }

void createDirectories() {
  if (!sd_initialized)
    return;

  if (!SD.exists(Hardware::SD::PROFILES_DIR)) {
    SD.mkdir(Hardware::SD::PROFILES_DIR);
    Serial.println("[ProfileManager] Creado directorio /profiles");
  }

  if (!SD.exists(Hardware::SD::TRANSFORMADOR_DIR)) {
    SD.mkdir(Hardware::SD::TRANSFORMADOR_DIR);
    Serial.println(
        "[ProfileManager] Creado directorio /profiles/transformador");
  }

  if (!SD.exists(Hardware::SD::HONEYCOMB_DIR)) {
    SD.mkdir(Hardware::SD::HONEYCOMB_DIR);
    Serial.println("[ProfileManager] Creado directorio /profiles/honeycomb");
  }
}

String sanitizeFilename(const String &nombre) {
  String sanitized = nombre;
  sanitized.replace(" ", "_");
  sanitized.replace("/", "_");
  sanitized.replace("\\", "_");
  sanitized.replace(":", "_");
  sanitized.replace("*", "_");
  sanitized.replace("?", "_");
  sanitized.replace("\"", "_");
  sanitized.replace("<", "_");
  sanitized.replace(">", "_");
  sanitized.replace("|", "_");

  if (sanitized.length() > 20) {
    sanitized = sanitized.substring(0, 20);
  }

  return sanitized;
}

// =========================================================================
// FUNCIONES PARA TRANSFORMADOR
// =========================================================================

bool saveTransformadorProfile(const char *nombre,
                              const ConfigTransformador &config) {
  if (!sd_initialized) {
    Serial.println("[ProfileManager] ERROR: SD no inicializada");
    return false;
  }

  String filename = String(Hardware::SD::TRANSFORMADOR_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    Serial.printf("[ProfileManager] ERROR: No se pudo crear %s\n",
                  filename.c_str());
    return false;
  }

  // Escribir configuración en formato legible
  file.println("# Perfil Transformador");
  file.printf("nombre=%s\n", nombre);
  file.printf("diametro_alambre_mm=%.3f\n", config.diametro_alambre_mm);
  file.printf("longitud_bobinado_mm=%.2f\n", config.longitud_bobinado_mm);
  file.printf("vueltas_total=%lu\n", config.vueltas_total);
  file.printf("velocidad_rpm=%.1f\n", config.velocidad_rpm);
  file.printf("detener_en_capas=%d\n", config.detener_en_capas ? 1 : 0);

  file.close();

  Serial.printf("[ProfileManager] Guardado: %s\n", filename.c_str());
  return true;
}

bool loadTransformadorProfile(const char *nombre, ConfigTransformador &config) {
  if (!sd_initialized) {
    Serial.println("[ProfileManager] ERROR: SD no inicializada");
    return false;
  }

  String filename = String(Hardware::SD::TRANSFORMADOR_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  File file = SD.open(filename.c_str(), FILE_READ);
  if (!file) {
    Serial.printf("[ProfileManager] ERROR: No se pudo abrir %s\n",
                  filename.c_str());
    return false;
  }

  // Leer línea por línea
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("#") || line.length() == 0) {
      continue;
    }

    int separatorIndex = line.indexOf('=');
    if (separatorIndex == -1) {
      continue;
    }

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    if (key == "diametro_alambre_mm") {
      config.diametro_alambre_mm = value.toFloat();
    } else if (key == "longitud_bobinado_mm") {
      config.longitud_bobinado_mm = value.toFloat();
    } else if (key == "vueltas_total") {
      config.vueltas_total = value.toInt();
    } else if (key == "velocidad_rpm") {
      config.velocidad_rpm = value.toFloat();
    } else if (key == "detener_en_capas") {
      config.detener_en_capas = (value.toInt() == 1);
    }
  }

  file.close();

  // Recalcular parámetros
  config.calcular_parametros();

  Serial.printf("[ProfileManager] Cargado: %s\n", filename.c_str());
  return true;
}

bool deleteTransformadorProfile(const char *nombre) {
  if (!sd_initialized) {
    return false;
  }

  String filename = String(Hardware::SD::TRANSFORMADOR_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  if (SD.remove(filename.c_str())) {
    Serial.printf("[ProfileManager] Eliminado: %s\n", filename.c_str());
    return true;
  }

  return false;
}

std::vector<ProfileInfo> listTransformadorProfiles() {
  std::vector<ProfileInfo> profiles;

  if (!sd_initialized) {
    return profiles;
  }

  File dir = SD.open(Hardware::SD::TRANSFORMADOR_DIR);
  if (!dir) {
    return profiles;
  }

  File file = dir.openNextFile();
  while (file && profiles.size() < Hardware::SD::MAX_PROFILES) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".txt")) {
        ProfileInfo info;

        // Extraer nombre del archivo (sin extensión)
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash != -1) {
          filename = filename.substring(lastSlash + 1);
        }
        filename.replace(".txt", "");

        strncpy(info.nombre, filename.c_str(), sizeof(info.nombre) - 1);
        info.nombre[sizeof(info.nombre) - 1] = '\0';
        info.modo = ModoBobinado::TRANSFORMADOR;
        info.existe = true;

        profiles.push_back(info);
      }
    }
    file = dir.openNextFile();
  }

  dir.close();
  return profiles;
}

// =========================================================================
// FUNCIONES PARA NIDO DE ABEJA
// =========================================================================

bool saveHoneycombProfile(const char *nombre, const ConfigNidoAbeja &config) {
  if (!sd_initialized) {
    Serial.println("[ProfileManager] ERROR: SD no inicializada");
    return false;
  }

  String filename = String(Hardware::SD::HONEYCOMB_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    Serial.printf("[ProfileManager] ERROR: No se pudo crear %s\n",
                  filename.c_str());
    return false;
  }

  // Escribir configuración
  file.println("# Perfil Nido de Abeja");
  file.printf("nombre=%s\n", nombre);
  file.printf("diametro_hilo=%.3f\n", config.diametro_hilo);
  file.printf("diametro_carrete=%.2f\n", config.diametro_carrete);
  file.printf("ancho_carrete=%.2f\n", config.ancho_carrete);
  file.printf("desfase_grados=%.2f\n", config.desfase_grados);
  file.printf("num_vueltas=%lu\n", config.num_vueltas);
  file.printf("velocidad_rpm=%.1f\n", config.velocidad_rpm);

  file.close();

  Serial.printf("[ProfileManager] Guardado: %s\n", filename.c_str());
  return true;
}

bool loadHoneycombProfile(const char *nombre, ConfigNidoAbeja &config) {
  if (!sd_initialized) {
    Serial.println("[ProfileManager] ERROR: SD no inicializada");
    return false;
  }

  String filename = String(Hardware::SD::HONEYCOMB_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  File file = SD.open(filename.c_str(), FILE_READ);
  if (!file) {
    Serial.printf("[ProfileManager] ERROR: No se pudo abrir %s\n",
                  filename.c_str());
    return false;
  }

  // Leer línea por línea
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("#") || line.length() == 0) {
      continue;
    }

    int separatorIndex = line.indexOf('=');
    if (separatorIndex == -1) {
      continue;
    }

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    if (key == "diametro_hilo") {
      config.diametro_hilo = value.toFloat();
    } else if (key == "diametro_carrete") {
      config.diametro_carrete = value.toFloat();
    } else if (key == "ancho_carrete") {
      config.ancho_carrete = value.toFloat();
    } else if (key == "desfase_grados") {
      config.desfase_grados = value.toFloat();
    } else if (key == "num_vueltas") {
      config.num_vueltas = value.toInt();
    } else if (key == "velocidad_rpm") {
      config.velocidad_rpm = value.toFloat();
    }
  }

  file.close();

  // Recalcular parámetros
  config.calcular_parametros();

  Serial.printf("[ProfileManager] Cargado: %s\n", filename.c_str());
  return true;
}

bool deleteHoneycombProfile(const char *nombre) {
  if (!sd_initialized) {
    return false;
  }

  String filename = String(Hardware::SD::HONEYCOMB_DIR) + "/" +
                    sanitizeFilename(nombre) + ".txt";

  if (SD.remove(filename.c_str())) {
    Serial.printf("[ProfileManager] Eliminado: %s\n", filename.c_str());
    return true;
  }

  return false;
}

std::vector<ProfileInfo> listHoneycombProfiles() {
  std::vector<ProfileInfo> profiles;

  if (!sd_initialized) {
    return profiles;
  }

  File dir = SD.open(Hardware::SD::HONEYCOMB_DIR);
  if (!dir) {
    return profiles;
  }

  File file = dir.openNextFile();
  while (file && profiles.size() < Hardware::SD::MAX_PROFILES) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".txt")) {
        ProfileInfo info;

        // Extraer nombre del archivo (sin extensión)
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash != -1) {
          filename = filename.substring(lastSlash + 1);
        }
        filename.replace(".txt", "");

        strncpy(info.nombre, filename.c_str(), sizeof(info.nombre) - 1);
        info.nombre[sizeof(info.nombre) - 1] = '\0';
        info.modo = ModoBobinado::NIDO_ABEJA;
        info.existe = true;

        profiles.push_back(info);
      }
    }
    file = dir.openNextFile();
  }

  dir.close();
  return profiles;
}

} // namespace ProfileManager
