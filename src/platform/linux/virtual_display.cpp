#include "virtual_display.h"

#include <array>  // For std::array
#include <chrono>  // For std::chrono::milliseconds
#include <cstdio>  // For popen, pclose
#include <iostream>
#include <memory>  // For std::unique_ptr
#include <sstream>  // For std::stringstream
#include <stdexcept>
#include <string>
#include <thread>  // For std::this_thread::sleep_for

// Define the global handle for the EVDI device.
// It will be initialized in openVDisplayDevice().
evdi_handle VDISPLAY::vdisplay_handle = nullptr;

// --- Helper Function ---
/**
 * @brief Executes a command and returns its standard output.
 * @param cmd The command to execute.
 * @return The standard output of the command as a string.
 */
std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

namespace VDISPLAY {

  // --- Core Lifecycle Functions ---

  DRIVER_STATUS openVDisplayDevice() {
    // If we already have a valid handle, there's nothing to do.
    if (vdisplay_handle) {
      std::cout << "[VDISPLAY-Linux] EVDI device is already open." << std::endl;
      return DRIVER_STATUS::OK;
    }

    std::cout << "[VDISPLAY-Linux] Searching for EVDI device..." << std::endl;

    // The EVDI devices appear as DRM devices in /dev/dri/
    const std::string drm_dir = "/dev/dri/";
    int card_number = -1;

    for (const auto &entry : std::filesystem::directory_iterator(drm_dir)) {
      const std::string path = entry.path().string();
      if (path.rfind(drm_dir + "card", 0) == 0) {
        try {
          // Attempt to open the device using libevdi
          vdisplay_handle = evdi_open(std::stoi(path.substr((drm_dir + "card").length())));

          if (vdisplay_handle) {
            card_number = std::stoi(path.substr((drm_dir + "card").length()));
            std::cout << "[VDISPLAY-Linux] Found EVDI device at: " << path << std::endl;
            break;  // Found a device, exit the loop
          }
        } catch (const std::invalid_argument &ia) {
          // Ignore files that don't match the cardX pattern
          continue;
        }
      }
    }

    // Check if we found and opened a device
    if (!vdisplay_handle) {
      std::cerr << "[VDISPLAY-Linux] ERROR: No EVDI device found. Is the evdi kernel module loaded?" << std::endl;
      return DRIVER_STATUS::FAILED;
    }

    // After opening, it's crucial to check for version compatibility
    // between the libevdi library and the loaded kernel module.
    if (!evdi_check_version(vdisplay_handle)) {
      std::cerr << "[VDISPLAY-Linux] ERROR: EVDI library and kernel module version mismatch." << std::endl;
      const char *kernel_version = "N/A";
      // A more robust implementation could try to get the actual kernel version.
      std::cerr << "  - Library version: " << evdi_get_lib_version() << std::endl;

      // Clean up the handle before failing
      evdi_close(vdisplay_handle);
      vdisplay_handle = nullptr;
      return DRIVER_STATUS::VERSION_INCOMPATIBLE;
    }

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

    // Step 1: Define the display mode for EVDI
    evdi_mode mode = {
      .width = (int32_t) width,
      .height = (int32_t) height,
      .refresh_rate = (int32_t) fps,
      .bits_per_pixel = 32,
      .pixel_format = 0x20203852,  // DRM_FORMAT_RGB888 - A common pixel format
    };

    // Step 2: Connect the virtual display
    // This makes the kernel aware of a new "monitor".
    if (!evdi_connect(vdisplay_handle, &mode)) {
      std::cerr << "[VDISPLAY-Linux] ERROR: Failed to connect EVDI display." << std::endl;
      return "";
    }

    std::cout << "[VDISPLAY-Linux] EVDI display connected. Configuring with xrandr..." << std::endl;
    // Give the system a moment to recognize the new device.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 3: Use xrandr to configure the new display.
    try {
      // Get the name of the newly connected display. We assume it's the first "connected" but not "primary" one.
      // A more robust solution might compare xrandr output before and after connecting.
      std::string displayName = exec("xrandr -q | grep ' connected' | grep -v 'primary' | awk '{print $1}' | head -n 1");
      displayName.erase(displayName.find_last_not_of(" \n\r\t") + 1);  // Trim whitespace

      if (displayName.empty()) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not find the name of the new virtual display." << std::endl;
        return "";
      }

      std::cout << "[VDISPLAY-Linux] Found new display output: " << displayName << std::endl;

      // Create a modeline for the desired resolution and refresh rate using `cvt`.
      std::string modeName = std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string(fps);
      std::string cvt_cmd = "cvt " + std::to_string(width) + " " + std::to_string(height) + " " + std::to_string(fps);
      std::string cvt_output = exec(cvt_cmd.c_str());

      // Extract the modeline details from the cvt output
      size_t modeline_start = cvt_output.find("Modeline");
      if (modeline_start == std::string::npos) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not generate modeline with cvt." << std::endl;
        return "";
      }
      std::string modeline = cvt_output.substr(modeline_start + strlen("Modeline "));
      modeline.erase(0, modeline.find_first_not_of(" \n\r\t"));  // Trim leading whitespace

      // Step 4: Apply the configuration using xrandr
      std::string cmd;

      // Add the new mode
      cmd = "xrandr --newmode " + modeline;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      // Add the mode to the display
      cmd = "xrandr --addmode " + displayName + " " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      // Set the mode on the display
      cmd = "xrandr --output " + displayName + " --mode " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      system(cmd.c_str());

      std::cout << "[VDISPLAY-Linux] Virtual display '" << displayName << "' created and configured." << std::endl;
      return displayName;

    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR during xrandr configuration: " << e.what() << std::endl;
      // If xrandr fails, we should disconnect the EVDI display to clean up.
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
      // Step 1: Find the display name associated with the EVDI device.
      // We assume the non-primary connected display is our virtual one.
      std::string displayName = exec("xrandr -q | grep ' connected' | grep -v 'primary' | awk '{print $1}' | head -n 1");
      displayName.erase(displayName.find_last_not_of(" \n\r\t") + 1);  // Trim whitespace

      if (!displayName.empty()) {
        // Step 2: Turn off the display output using xrandr.
        std::string cmd = "xrandr --output " + displayName + " --off";
        std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
        system(cmd.c_str());
      } else {
        std::cout << "[VDISPLAY-Linux] Warning: Could not find virtual display name to turn off via xrandr. Proceeding with EVDI disconnect." << std::endl;
      }

    } catch (const std::runtime_error &e) {
      std::cerr << "[VDISPLAY-Linux] ERROR during xrandr configuration for removal: " << e.what() << std::endl;
      // We can still attempt to disconnect EVDI even if xrandr fails.
    }

    // Step 3: Disconnect the display from the EVDI handle.
    // This is the most critical step to release the virtual device.
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
      // Step 1: Generate a unique name and modeline for the new mode.
      std::string modeName = std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string(refresh_rate);
      std::string cvt_cmd = "cvt " + std::to_string(width) + " " + std::to_string(height) + " " + std::to_string(refresh_rate);
      std::string cvt_output = exec(cvt_cmd.c_str());

      size_t modeline_start = cvt_output.find("Modeline");
      if (modeline_start == std::string::npos) {
        std::cerr << "[VDISPLAY-Linux] ERROR: Could not generate modeline with cvt." << std::endl;
        return -1;
      }
      std::string modeline = cvt_output.substr(modeline_start + strlen("Modeline "));
      modeline.erase(0, modeline.find_first_not_of(" \n\r\t"));

      // Step 2: Apply the configuration using xrandr
      std::string cmd;

      // Create the new mode
      cmd = "xrandr --newmode " + modeline;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      if (system(cmd.c_str()) != 0) {
        std::cerr << "[VDISPLAY-Linux] ERROR: xrandr --newmode failed." << std::endl;
        return -1;
      }

      // Add the mode to the target display
      cmd = "xrandr --addmode " + std::string(deviceName) + " " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      if (system(cmd.c_str()) != 0) {
        std::cerr << "[VDISPLAY-Linux] ERROR: xrandr --addmode failed." << std::endl;
        // Best effort to remove the mode we just created
        system(("xrandr --delmode " + std::string(deviceName) + " " + modeName).c_str());
        return -1;
      }

      // Set the new mode on the display
      cmd = "xrandr --output " + std::string(deviceName) + " --mode " + modeName;
      std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
      if (system(cmd.c_str()) != 0) {
        std::cerr << "[VDISPLAY-Linux] ERROR: xrandr --output --mode failed." << std::endl;
        return -1;
      }

      std::cout << "[VDISPLAY-Linux] Successfully changed mode for " << deviceName << "." << std::endl;
      return 0;  // Success

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

    // Construct the xrandr command
    std::string cmd = "xrandr --output " + std::string(primaryDeviceName) + " --primary";

    // Execute the command
    std::cout << "[VDISPLAY-Linux] Executing: " << cmd << std::endl;
    int result = system(cmd.c_str());

    // Check the result
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
      // Execute `xrandr -q` and capture the output
      std::string xrandr_output = exec("xrandr -q");
      std::stringstream ss(xrandr_output);
      std::string line;

      // Read the output line by line
      while (std::getline(ss, line)) {
        // Check if the line contains the word "primary"
        if (line.find(" primary ") != std::string::npos) {
          // The display name is the first word on the line
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
