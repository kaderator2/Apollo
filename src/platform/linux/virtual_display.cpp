#include "virtual_display.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

evdi_handle VDISPLAY::vdisplay_handle = nullptr;

// --- Helper Functions ---

/**
 * @brief Executes a command and returns its standard output.
 * @param cmd The command to execute.
 * @return The standard output of the command as a string.
 */
std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

static std::set<std::string> get_connected_displays() {
  std::set<std::string> displays;
  try {
    std::string xrandr_output = exec("xrandr -q");
    std::stringstream ss(xrandr_output);
    std::string line;
    std::regex device_regex(R"(^(\S+)\s+connected)");
    std::smatch match;

    while (std::getline(ss, line)) {
      if (std::regex_search(line, match, device_regex)) {
        displays.insert(match[1].str());
      }
    }
  } catch (const std::runtime_error &e) {
    std::cerr << "[VDISPLAY-Linux] ERROR executing xrandr: " << e.what() << std::endl;
  }
  return displays;
}

namespace VDISPLAY {

  // --- Core Lifecycle Functions ---

  DRIVER_STATUS openVDisplayDevice() {
    if (vdisplay_handle) {
      std::cout << "[VDISPLAY-Linux] EVDI device is already open." << std::endl;
      return DRIVER_STATUS::OK;
    }

    std::cout << "[VDISPLAY-Linux] Searching for EVDI device..." << std::endl;

    const std::string drm_dir = "/dev/dri/";
    int card_number = -1;

    for (const auto &entry : std::filesystem::directory_iterator(drm_dir)) {
      const std::string path = entry.path().string();
      if (path.rfind(drm_dir + "card", 0) == 0) {
        try {
          int current_card_number = std::stoi(path.substr((drm_dir + "card").length()));
          // --- ADD THIS LOGGING ---
          std::cout << "[VDISPLAY-Linux-DEBUG] Attempting to open /dev/dri/card" << current_card_number << std::endl;

          // Temporarily store the handle to check if it's valid
          evdi_handle temp_handle = evdi_open(current_card_number);

          if (temp_handle) {
            // --- ADD THIS LOGGING ---
            std::cout << "[VDISPLAY-Linux-DEBUG] Successfully received handle for card" << current_card_number << std::endl;
            vdisplay_handle = temp_handle;
            card_number = current_card_number;
            std::cout << "[VDISPLAY-Linux] Found EVDI device at: " << path << std::endl;
            break;
          } else {
            // --- ADD THIS LOGGING ---
            std::cerr << "[VDISPLAY-Linux-DEBUG] Failed to open /dev/dri/card" << current_card_number << ". Handle is null." << std::endl;
          }
        } catch (const std::invalid_argument &ia) {
          // This is fine, just ignore non-numeric card suffixes
          continue;
        }
      }
    }

    if (!vdisplay_handle) {
      std::cerr << "[VDISPLAY-Linux] ERROR: No EVDI device found. Is the evdi kernel module loaded?" << std::endl;
      std::cerr << "[VDISPLAY-Linux] SUGGESTION: Check for version mismatch between libevdi and the kernel module. Also check Xorg/Wayland logs." << std::endl;

      return DRIVER_STATUS::FAILED;
    }

    evdi_lib_version lib_ver;
    evdi_get_lib_version(&lib_ver);
    std::cout << "[VDISPLAY-Linux] EVDI library version: " << lib_ver.version_major << "." << lib_ver.version_minor << "." << lib_ver.version_patchlevel << std::endl;

    std::cout << "[VDISPLAY-Linux] EVDI device opened successfully on card" << card_number << "." << std::endl;
    return DRIVER_STATUS::OK;
  }

  void closeVDisplayDevice() {
    if (vdisplay_handle) {
      std::cout << "[VDISPLAY-Linux] Closing EVDI device." << std::endl;
      evdi_close(vdisplay_handle);
      vdisplay_handle = nullptr;
    } else {
      std::cout << "[VDISPLAY-Linux] closeVDisplayDevice() called, but device was not open." << std::endl;
    }
  }

  // --- Virtual Display Management ---

  std::string createVirtualDisplay(
    const char *s_client_uid,
    const char *s_client_name,
    uint32_t width,
    uint32_t height,
    uint32_t fps
  ) {
    if (!vdisplay_handle) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Cannot create display, EVDI device not open." << std::endl;
      return "";
    }

    std::cout << "[VDISPLAY-Linux] Creating virtual display for client: " << s_client_uid << std::endl;

    // --- NEW LOGIC: Reliably find the new display ---
    // 1. Get the set of displays BEFORE we connect
    auto displays_before = get_connected_displays();

    // Use a standard EDID block for evdi_connect.
    // This is a standard 128-byte EDID for a generic 1920x1080 display.
    const unsigned char edid[] = {
      0x00,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0x00,
      0x1A,
      0x2B,
      0x3C,
      0x4D,
      0x01,
      0x00,
      0x00,
      0x00,
      0x1C,
      0x17,
      0x01,
      0x03,
      0x80,
      0x00,
      0x00,
      0x78,
      0x0A,
      0xEE,
      0x91,
      0xA3,
      0x54,
      0x4C,
      0x99,
      0x26,
      0x0F,
      0x50,
      0x54,
      0x00,
      0x00,
      0x00,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x01,
      0x02,
      0x3A,
      0x80,
      0x18,
      0x71,
      0x38,
      0x2D,
      0x40,
      0x58,
      0x2C,
      0x45,
      0x00,
      0xFD,
      0x1E,
      0x11,
      0x00,
      0x00,
      0x1E,
      0x00,
      0x00,
      0x00,
      0xFF,
      0x00,
      0x41,
      0x70,
      0x6F,
      0x6C,
      0x6C,
      0x6F,
      0x56,
      0x44,
      0x49,
      0x0A,
      0x20,
      0x20,
      0x20,
      0x00,
      0x00,
      0x00,
      0xFD,
      0x00,
      0x38,
      0x4C,
      0x1E,
      0x53,
      0x11,
      0x00,
      0x0A,
      0x20,
      0x20,
      0x20,
      0x20,
      0x20,
      0x20,
      0x00,
      0x00,
      0x00,
      0xFC,
      0x00,
      0x41,
      0x70,
      0x6F,
      0x6C,
      0x6C,
      0x6F,
      0x0A,
      0x20,
      0x20,
      0x20,
      0x20,
      0x20,
      0x20,
      0x01,
      0x2D
    };

    // Connect the virtual display.
    // 2. Connect the virtual display.
    evdi_connect(vdisplay_handle, edid, sizeof(edid), 0);
    std::cout << "[VDISPLAY-Linux] EVDI display connected. Searching for new display output..." << std::endl;

    // 3. Find the NEW display by comparing the lists
    std::string displayName;
    for (int i = 0; i < 5; ++i) {  // Retry for a few seconds
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto displays_after = get_connected_displays();
      std::set<std::string> new_displays;
      std::set_difference(displays_after.begin(), displays_after.end(), displays_before.begin(), displays_before.end(), std::inserter(new_displays, new_displays.begin()));

      if (!new_displays.empty()) {
        displayName = *new_displays.begin();
        break;
      }
    }
    // --- END OF NEW LOGIC ---

    if (displayName.empty()) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Could not find the name of the new virtual display." << std::endl;
      evdi_disconnect(vdisplay_handle);
      return "";
    }

    std::cout << "[VDISPLAY-Linux] Found new virtual display: " << displayName << std::endl;

    std::cout << "[VDISPLAY-Linux] EVDI display connected. Configuring with xrandr..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    try {
      std::string displayName = exec("xrandr -q | grep ' connected' | grep -v 'primary' | awk '{print $1}' | head -n 1");
      displayName.erase(displayName.find_last_not_of(" \n\r\t") + 1);

      if (displayName.empty()) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not find the name of the new virtual display." << std::endl;
        evdi_disconnect(vdisplay_handle);  // Clean up
        return "";
      }

      std::cout << "[VDISPLAY-Linux] Found new display output: " << displayName << std::endl;

      std::string cvt_cmd = "cvt " + std::to_string(width) + " " + std::to_string(height) + " " + std::to_string(fps);
      std::string cvt_output = exec(cvt_cmd.c_str());

      size_t modeline_start = cvt_output.find("Modeline");
      if (modeline_start == std::string::npos) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not generate modeline with cvt." << std::endl;
        evdi_disconnect(vdisplay_handle);  // Clean up
        return "";
      }
      std::string modeline = cvt_output.substr(modeline_start + strlen("Modeline "));
      modeline.erase(0, modeline.find_first_not_of(" \n\r\t"));

      // --- NEW: Extract the exact mode name from the modeline ---
      std::string modeName = modeline.substr(0, modeline.find(" "));
      modeName.erase(std::remove(modeName.begin(), modeName.end(), '"'), modeName.end());

      std::string cmd;
      system(("xrandr --delmode " + displayName + " " + modeName).c_str());

      cmd = "xrandr --newmode " + modeline;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      // Use the correctly parsed modeName
      cmd = "xrandr --addmode " + displayName + " " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      // Use the correctly parsed modeName
      cmd = "xrandr --output " + displayName + " --mode " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      std::cout << "[VDISPLAY-Linux] Waiting for display to stabilize..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(6));  // 1-second delay

      std::cout << "[VDISPLAY-Linux] Virtual display '" << displayName << "' created and configured." << std::endl;
      return displayName;

    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR during xrandr configuration: " << e.what() << std::endl;
      evdi_disconnect(vdisplay_handle);
      return "";
    }
  }

  bool removeVirtualDisplay(const char *client_uid) {
    if (!vdisplay_handle) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Cannot remove display, EVDI device is not open." << std::endl;
      return false;
    }

    std::cout << "[VDISPLAY-Linux] Removing virtual display for client: " << client_uid << std::endl;

    try {
      std::string displayName = exec("xrandr -q | grep ' connected' | grep -v 'primary' | awk '{print $1}' | head -n 1");
      displayName.erase(displayName.find_last_not_of(" \n\r\t") + 1);

      if (!displayName.empty()) {
        std::string cmd = "xrandr --output " + displayName + " --off";
        std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
        system(cmd.c_str());
      } else {
        std::cout << "[VDISPLAY-Linux] Warning: Could not find virtual display name to turn off via xrandr. Proceeding with EVDI disconnect." << std::endl;
      }

    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR during xrandr configuration for removal: " << e.what() << std::endl;
    }

    evdi_disconnect(vdisplay_handle);
    std::cout << "[VDISPLAY-Linux] EVDI display disconnected." << std::endl;

    return true;
  }

  // --- Display Configuration ---

  int changeDisplaySettings(const char *deviceName, int width, int height, int refresh_rate) {
    if (!deviceName || std::string(deviceName).empty()) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Invalid deviceName provided to changeDisplaySettings." << std::endl;
      return -1;
    }

    std::cout << "[VDISPLAY-Linux] Changing display settings for " << deviceName << " to "
              << width << "x" << height << " @ " << refresh_rate << "Hz" << std::endl;

    try {
      std::string cvt_cmd = "cvt " + std::to_string(width) + " " + std::to_string(height) + " " + std::to_string(refresh_rate);
      std::string cvt_output = exec(cvt_cmd.c_str());

      size_t modeline_start = cvt_output.find("Modeline");
      if (modeline_start == std::string::npos) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not generate modeline with cvt." << std::endl;
        return -1;
      }
      std::string modeline = cvt_output.substr(modeline_start + strlen("Modeline "));
      modeline.erase(0, modeline.find_first_not_of(" \n\r\t"));

      std::string modeName = modeline.substr(0, modeline.find(" "));
      modeName.erase(std::remove(modeName.begin(), modeName.end(), '"'), modeName.end());

      // --- REVISED LOGIC ---

      // 1. Create the mode. We don't check for errors because it might already exist, which is fine.
      std::string newmode_cmd = "xrandr --newmode " + modeline;
      std::cout << "[VDISPLAY-Linux] Executing: " << newmode_cmd << std::endl;
      system(newmode_cmd.c_str());

      // 2. Add the mode to the display. We don't check for errors because it might already be added.
      std::string addmode_cmd = "xrandr --addmode " + std::string(deviceName) + " " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << addmode_cmd << std::endl;
      system(addmode_cmd.c_str());

      // 3. This is the only command that MUST succeed. It activates the mode.
      std::string output_cmd = "xrandr --output " + std::string(deviceName) + " --mode " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << output_cmd << std::endl;
      if (system(output_cmd.c_str()) != 0) {
        std::cerr << "[VDISPLAY-Linux] ERROR: xrandr --output --mode failed." << std::endl;
        // As a last resort, try to just turn the display on automatically
        system(("xrandr --output " + std::string(deviceName) + " --auto").c_str());
        return -1;
      }

      std::cout << "[VDISPLAY-Linux] Successfully changed mode for " << deviceName << "." << std::endl;
      return 0;

    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR during display settings change: " << e.what() << std::endl;
      return -1;
    }
  }

  bool setPrimaryDisplay(const char *primaryDeviceName) {
    if (!primaryDeviceName || std::string(primaryDeviceName).empty()) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Invalid primaryDeviceName provided." << std::endl;
      return false;
    }

    std::cout << "[VDISPLAY-Linux] Setting " << primaryDeviceName << " as the primary display." << std::endl;

    std::string cmd = "xrandr --output " + std::string(primaryDeviceName) + " --primary";

    std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
    int result = system(cmd.c_str());

    if (result != 0) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Failed to set primary display. xrandr command failed." << std::endl;
      return false;
    }

    std::cout << "[VDISPLAY-Linux] Primary display set successfully." << std::endl;
    return true;
  }

  std::string getPrimaryDisplay() {
    std::cout << "[VDISPLAY-Linux] Getting primary display..." << std::endl;
    try {
      std::string xrandr_output = exec("xrandr -q");
      std::stringstream ss(xrandr_output);
      std::string line;

      while (std::getline(ss, line)) {
        if (line.find(" primary ") != std::string::npos) {
          std::string deviceName = line.substr(0, line.find(' '));
          std::cout << "[VDISPLAY-Linux] Found primary display: " << deviceName << std::endl;
          return deviceName;
        }
      }
    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR executing xrandr to get primary display: " << e.what() << std::endl;
      return "";
    }

    std::cerr << "[VDISPLAY-Linux] ERROR: Could not find a primary display." << std::endl;
    return "";
  }

}  // namespace VDISPLAY
