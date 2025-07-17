#include "settings_manager.h"

#include "virtual_display.h"  // Your low-level implementation

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <variant>

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

// --- Helper Function ---
/**
 * @brief Executes a command and returns its standard output.
 * @param cmd The command to execute.
 * @return The standard output of the command as a string.
 */
static std::string exec(const char *cmd) {
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

LinuxSettingsManager::LinuxSettingsManager() {
  if (VDISPLAY::openVDisplayDevice() != VDISPLAY::DRIVER_STATUS::OK) {
    std::cerr << "[LinuxSettingsManager] ERROR: Failed to open EVDI device during initialization." << std::endl;
    return;
  }

  std::cout << "[LinuxSettingsManager] EVDI device opened successfully." << std::endl;
  std::cout << "[LinuxSettingsManager] Capturing initial display configuration..." << std::endl;

  try {
    std::string xrandr_output = exec("xrandr -q");
    std::stringstream ss(xrandr_output);
    std::string line;
    display_device::SingleDisplayConfiguration current_display;
    bool new_display = false;

    while (std::getline(ss, line)) {
      if (line.find(" connected") != std::string::npos) {
        if (new_display) {
          m_original_config.push_back(current_display);
        }

        current_display = {};
        current_display.m_device_id = line.substr(0, line.find(' '));

        auto res = parse_resolution(line);
        if (res) {
          current_display.m_resolution = *res;
        }
        new_display = true;

      } else if (new_display && line.find('*') != std::string::npos) {
        std::stringstream line_ss(line);
        std::string segment;
        line_ss >> segment;
        line_ss >> segment;

        try {
          segment.erase(std::remove(segment.begin(), segment.end(), '*'), segment.end());
          segment.erase(std::remove(segment.begin(), segment.end(), '+'), segment.end());
          float rate = std::stof(segment);
          current_display.m_refresh_rate = display_device::Rational {static_cast<unsigned int>(rate * 1000), 1000};
        } catch (const std::exception &e) {
          // Ignore if parsing fails
        }
      }
    }
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
  revertSettings();
  VDISPLAY::closeVDisplayDevice();
  std::cout << "[LinuxSettingsManager] Cleanup complete." << std::endl;
}

display_device::EnumeratedDeviceList LinuxSettingsManager::enumAvailableDevices() const {
  std::cout << "[LinuxSettingsManager] Enumerating available devices..." << std::endl;
  display_device::EnumeratedDeviceList device_list;

  try {
    std::string xrandr_output = exec("xrandr -q");
    std::stringstream ss(xrandr_output);
    std::string line;

    // Regex to find a new connected device, e.g., "HDMI-1 connected ..."
    std::regex device_regex(R"(^(\S+)\s+connected)");
    std::smatch match;

    while (std::getline(ss, line)) {
      // Check if the line indicates a new connected device
      if (std::regex_search(line, match, device_regex)) {
        display_device::EnumeratedDevice current_device;
        current_device.m_device_id = match[1].str();
        current_device.m_friendly_name = current_device.m_device_id;  // On Linux, these are the same

        // The EnumeratedDevice struct does not have a list of available modes.
        // We will simply add the device itself to the list.
        device_list.push_back(current_device);
      }
    }

    std::cout << "[LinuxSettingsManager] Found " << device_list.size() << " connected devices." << std::endl;

  } catch (const std::runtime_error &e) {
    std::cerr << "[LinuxSettingsManager] ERROR executing xrandr to enumerate devices: " << e.what() << std::endl;
  }

  return device_list;
}

std::string LinuxSettingsManager::getDisplayName(const std::string &device_id) const {
  std::cout << "[LinuxSettingsManager] getDisplayName() called for: " << device_id << std::endl;
  return device_id;
}

LinuxSettingsManager::ApplyResult LinuxSettingsManager::applySettings(const display_device::SingleDisplayConfiguration &config) {
  std::cout << "[LinuxSettingsManager] Applying display settings..." << std::endl;
  std::string target_device_name;

  if (config.m_resolution && config.m_refresh_rate) {
    if (auto *rational_rate = std::get_if<display_device::Rational>(&(*config.m_refresh_rate))) {
      std::cout << "[LinuxSettingsManager] Configuration requires creating a virtual display." << std::endl;

      const char *client_uid = "linux_client";
      const char *client_name = "Apollo";

      target_device_name = VDISPLAY::createVirtualDisplay(
        client_uid,
        client_name,
        config.m_resolution->m_width,
        config.m_resolution->m_height,
        rational_rate->m_numerator / rational_rate->m_denominator
      );

      if (target_device_name.empty()) {
        std::cerr << "[LinuxSettingsManager] ERROR: Failed to create virtual display." << std::endl;
        return ApplyResult::DevicePrepFailed;
      }
    } else {
      std::cerr << "[LinuxSettingsManager] ERROR: Refresh rate is not a rational number." << std::endl;
      return ApplyResult::DisplayModePrepFailed;
    }
  } else {
    std::cerr << "[LinuxSettingsManager] ERROR: ApplySettings called without a valid resolution/refresh rate." << std::endl;
    return ApplyResult::DisplayModePrepFailed;
  }

  if (config.m_device_prep == display_device::SingleDisplayConfiguration::DevicePreparation::EnsurePrimary) {
    std::cout << "[LinuxSettingsManager] Setting new virtual display as primary..." << std::endl;

    if (!VDISPLAY::setPrimaryDisplay(target_device_name.c_str())) {
      std::cerr << "[LinuxSettingsManager] ERROR: Failed to set the new display as primary." << std::endl;
      return ApplyResult::PrimaryDevicePrepFailed;
    }
  }

  std::cout << "[LinuxSettingsManager] Display settings applied successfully." << std::endl;
  return ApplyResult::Ok;
}

LinuxSettingsManager::RevertResult LinuxSettingsManager::revertSettings() {
  std::cout << "[LinuxSettingsManager] Reverting display settings..." << std::endl;

  if (!VDISPLAY::removeVirtualDisplay("linux_client")) {
    std::cerr << "[LinuxSettingsManager] ERROR: Failed to remove the virtual display." << std::endl;
  }

  std::cout << "[LinuxSettingsManager] Restoring original display layout..." << std::endl;
  bool all_restored = true;
  for (const auto &config : m_original_config) {
    if (config.m_resolution && config.m_refresh_rate) {
      if (auto *rational_rate = std::get_if<display_device::Rational>(&(*config.m_refresh_rate))) {
        int result = VDISPLAY::changeDisplaySettings(
          config.m_device_id.c_str(),
          config.m_resolution->m_width,
          config.m_resolution->m_height,
          rational_rate->m_numerator / rational_rate->m_denominator
        );
        if (result != 0) {
          all_restored = false;
          std::cerr << "[LinuxSettingsManager] ERROR: Failed to restore mode for display: " << config.m_device_id << std::endl;
        }
      }
    }
  }

  if (!all_restored) {
    return RevertResult::RevertingDisplayModesFailed;
  }

  std::cout << "[LinuxSettingsManager] Display settings reverted successfully." << std::endl;
  return RevertResult::Ok;
}

bool LinuxSettingsManager::resetPersistence() {
  std::cout << "[LinuxSettingsManager] resetPersistence() called. No action needed on Linux." << std::endl;
  return true;
}
