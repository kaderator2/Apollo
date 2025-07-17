#include "settings_manager.h"

#include "virtual_display.h"  // Your low-level implementation

#include <iostream>
#include <regex>
#include <sstream>

/**
 * @brief Helper to parse a resolution string like "1920x1080".
 */
std::optional<display_device::Resolution> parse_resolution(const std::string &res_str) {
  std::regex re("(\\d+)x(\\d+)");
  std::smatch match;
  if (std::regex_search(res_str, match, re) && match.size() == 3) {
    return display_device::Resolution {
      static_cast<unsigned int>(std::stoul(match[1].str())),
      static_cast<unsigned int>(std::stoul(match[2].str()))
    };
  }
  return std::nullopt;
}

LinuxSettingsManager::LinuxSettingsManager() {
  // Step 1: Open the EVDI device
  if (VDISPLAY::openVDisplayDevice() != VDISPLAY::DRIVER_STATUS::OK) {
    std::cerr << "[LinuxSettingsManager] ERROR: Failed to open EVDI device during initialization." << std::endl;
    return;
  }

  std::cout << "[LinuxSettingsManager] EVDI device opened successfully." << std::endl;
  std::cout << "[LinuxSettingsManager] Capturing initial display configuration..." << std::endl;

  // Step 2: Get the current display state using xrandr
  try {
    std::string xrandr_output = exec("xrandr -q");
    std::stringstream ss(xrandr_output);
    std::string line;
    display_device::SingleDisplayConfiguration current_display;
    bool new_display = false;

    while (std::getline(ss, line)) {
      // A new display line looks like: "eDP-1 connected ..."
      if (line.find(" connected") != std::string::npos) {
        // If we were processing a previous display, save it.
        if (new_display) {
          m_original_config.push_back(current_display);
        }

        current_display = {};  // Reset for the new display
        current_display.m_device_id = line.substr(0, line.find(' '));

        // Try to parse resolution from the "connected" line itself
        auto res = parse_resolution(line);
        if (res) {
          current_display.m_resolution = *res;
        }
        new_display = true;

        // A line with the active refresh rate looks like: "  1920x1080     60.00*+ ..."
      } else if (new_display && line.find('*') != std::string::npos) {
        std::stringstream line_ss(line);
        std::string segment;
        line_ss >> segment;  // Read the resolution part (e.g., 1920x1080)
        line_ss >> segment;  // Read the refresh rate part

        try {
          // Refresh rate might have a '*' or '+'
          segment.erase(std::remove(segment.begin(), segment.end(), '*'), segment.end());
          segment.erase(std::remove(segment.begin(), segment.end(), '+'), segment.end());
          float rate = std::stof(segment);
          current_display.m_refresh_rate = display_device::Rational {static_cast<unsigned int>(rate * 1000), 1000};
        } catch (const std::exception &e) {
          // Ignore if parsing fails
        }
      }
    }
    // Add the last processed display
    if (new_display) {
      m_original_config.push_back(current_display);
    }

    std::cout << "[LinuxSettingsManager] Successfully captured initial state for " << m_original_config.size() << " displays." << std::endl;

  } catch (const std::runtime_error &e) {
    std::cerr << "[LinuxSettingsManager] ERROR: Failed to execute xrandr to get initial display state: " << e.what() << std::endl;
  }
}

LinuxSettingsManager::~LinuxSettingsManager() {
  std::cout << "[LinuxSettingsManager] Shutting down and cleaning up..." << std::endl;

  // Step 1: Revert any active display configurations.
  // This will remove our virtual display and restore the original layout.
  revertSettings();

  // Step 2: Close the handle to the EVDI device.
  VDISPLAY::closeVDisplayDevice();

  std::cout << "[LinuxSettingsManager] Cleanup complete." << std::endl;
}

display_device::EnumeratedDeviceList LinuxSettingsManager::enumAvailableDevices() {
  std::cout << "[LinuxSettingsManager] Enumerating available devices..." << std::endl;
  display_device::EnumeratedDeviceList device_list;

  try {
    std::string xrandr_output = exec("xrandr -q");
    std::stringstream ss(xrandr_output);
    std::string line;

    display_device::EnumeratedDevice current_device;
    bool is_processing_device = false;

    // Regex to find a new connected device, e.g., "HDMI-1 connected ..."
    std::regex device_regex(R"(^(\S+)\s+connected)");
    // Regex to find a display mode, e.g., "  1920x1080     59.94 ..."
    std::regex mode_regex(R"(^\s+(\d+x\d+)\s+([\d\.]+))");
    std::smatch match;

    while (std::getline(ss, line)) {
      // Check if the line indicates a new connected device
      if (std::regex_search(line, match, device_regex)) {
        // If we were already processing a device, save it to the list first.
        if (is_processing_device) {
          device_list.push_back(current_device);
        }

        // Start a new device
        current_device = {};
        current_device.m_device_id = match[1].str();
        current_device.m_friendly_name = current_device.m_device_id;  // On Linux, these are the same
        is_processing_device = true;

      } else if (is_processing_device && std::regex_search(line, match, mode_regex)) {
        // This line is a mode belonging to the current device
        display_device::DisplayMode mode;

        // Parse resolution
        auto res = parse_resolution(match[1].str());
        if (res) {
          mode.m_resolution = *res;
        }

        // Parse refresh rate
        try {
          float rate = std::stof(match[2].str());
          mode.m_refresh_rate = display_device::Rational {static_cast<unsigned int>(rate * 1000), 1000};
        } catch (const std::exception &e) {
          // Skip mode if refresh rate is invalid
          continue;
        }

        current_device.m_available_modes.push_back(mode);
      }
    }

    // Add the last device to the list after the loop finishes
    if (is_processing_device) {
      device_list.push_back(current_device);
    }

    std::cout << "[LinuxSettingsManager] Found " << device_list.size() << " connected devices." << std::endl;

  } catch (const std::runtime_error &e) {
    std::cerr << "[LinuxSettingsManager] ERROR executing xrandr to enumerate devices: " << e.what() << std::endl;
  }

  return device_list;
}

std::string LinuxSettingsManager::getDisplayName(const std::string &device_id) {
  std::cout << "[LinuxSettingsManager] getDisplayName() called for: " << device_id << std::endl;
  return device_id;
}

display_device::ApplyResult LinuxSettingsManager::applySettings(const display_device::SingleDisplayConfiguration &config) {
  std::cout << "[LinuxSettingsManager] Applying display settings..." << std::endl;

  // We'll primarily focus on creating a new virtual display.
  // The logic can be expanded to handle other device_prep options.

  std::string target_device_name;

  // Check if we need to create a virtual display.
  // For this implementation, we assume that's the primary goal.
  if (config.m_resolution && config.m_refresh_rate) {
    std::cout << "[LinuxSettingsManager] Configuration requires creating a virtual display." << std::endl;

    // The client UID and name are not critical for the Linux implementation
    const char *client_uid = "linux_client";
    const char *client_name = "Artemis";

    target_device_name = VDISPLAY::createVirtualDisplay(
      client_uid,
      client_name,
      config.m_resolution->m_width,
      config.m_resolution->m_height,
      config.m_refresh_rate->m_numerator / config.m_refresh_rate->m_denominator
    );

    if (target_device_name.empty()) {
      std::cerr << "[LinuxSettingsManager] ERROR: Failed to create virtual display." << std::endl;
      return display_device::ApplyResult::Failed;
    }
  } else {
    // If no resolution is specified, we might be changing an existing display.
    // For now, we'll treat this as a configuration error.
    std::cerr << "[LinuxSettingsManager] ERROR: ApplySettings called without a valid resolution/refresh rate." << std::endl;
    return display_device::ApplyResult::Failed;
  }

  // Now, check if this new display needs to be set as the primary one.
  if (config.m_device_prep == display_device::SingleDisplayConfiguration::DevicePreparation::EnsurePrimary) {
    std::cout << "[LinuxSettingsManager] Setting new virtual display as primary..." << std::endl;

    if (!VDISPLAY::setPrimaryDisplay(target_device_name.c_str())) {
      std::cerr << "[LinuxSettingsManager] ERROR: Failed to set the new display as primary." << std::endl;
      // This might not be a fatal error, so we can choose to continue or fail.
      // For now, we'll report failure.
      return display_device::ApplyResult::Failed;
    }
  }

  std::cout << "[LinuxSettingsManager] Display settings applied successfully." << std::endl;
  return display_device::ApplyResult::Ok;
}

display_device::RevertResult LinuxSettingsManager::revertSettings() {
  std::cout << "[LinuxSettingsManager] Reverting display settings..." << std::endl;

  // Step 1: Remove the virtual display we created.
  // The client UID is not used in the Linux implementation.
  if (!VDISPLAY::removeVirtualDisplay("linux_client")) {
    std::cerr << "[LinuxSettingsManager] ERROR: Failed to remove the virtual display." << std::endl;
    // Even if this fails, we can still try to restore the original layout.
  }

  // Step 2: Restore the original display layout using the stored config.
  std::cout << "[LinuxSettingsManager] Restoring original display layout..." << std::endl;
  bool all_restored = true;
  for (const auto &config : m_original_config) {
    if (config.m_resolution && config.m_refresh_rate) {
      int result = VDISPLAY::changeDisplaySettings(
        config.m_device_id.c_str(),
        config.m_resolution->m_width,
        config.m_resolution->m_height,
        config.m_refresh_rate->m_numerator / 1000  // Convert back to integer Hz
      );
      if (result != 0) {
        all_restored = false;
        std::cerr << "[LinuxSettingsManager] ERROR: Failed to restore mode for display: " << config.m_device_id << std::endl;
      }
    }
  }

  if (!all_restored) {
    return display_device::RevertResult::Failed;
  }

  std::cout << "[LinuxSettingsManager] Display settings reverted successfully." << std::endl;
  return display_device::RevertResult::Ok;
}

/**
 * @brief Resets any persistent state.
 *
 * On Linux, this implementation does not save state to a file,
 * so this function has nothing to do.
 *
 * @return Always returns true.
 */
bool LinuxSettingsManager::resetPersistence() {
  std::cout << "[LinuxSettingsManager] resetPersistence() called. No action needed on Linux." << std::endl;
  return true;
}
