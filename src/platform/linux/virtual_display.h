#pragma once

#include <evdi.h>  // The core library for Linux implementation
#include <functional>
#include <string>
#include <vector>

namespace VDISPLAY {
  enum class DRIVER_STATUS {
    UNKNOWN = 1,
    OK = 0,
    FAILED = -1,
    VERSION_INCOMPATIBLE = -2,
    WATCHDOG_FAILED = -3
  };

  // The handle to the EVDI device provided by libevdi.
  extern evdi_handle vdisplay_handle;

  struct DisplayMode {
    int width;
    int height;
    int refresh_rate;
  };

  // --- Core Lifecycle Functions ---
  DRIVER_STATUS openVDisplayDevice();
  void closeVDisplayDevice();

  // --- Virtual Display Management ---
  std::string createVirtualDisplay(const char *s_client_uid, const char *s_client_name, uint32_t width, uint32_t height, uint32_t fps);
  bool removeVirtualDisplay(const char *client_uid);

  // --- Display Configuration ---
  int changeDisplaySettings(const char *deviceName, int width, int height, int refresh_rate);
  bool setPrimaryDisplay(const char *primaryDeviceName);
  std::string getPrimaryDisplay();

  // --- Stubs for unimplemented features ---
  inline bool getDisplayHDRByName(const char *displayName) {
    return false;
  }

  inline bool setDisplayHDRByName(const char *displayName, bool enable) {
    return false;
  }

}  // namespace VDISPLAY
