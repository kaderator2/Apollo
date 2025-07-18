#pragma once

#include <display_device/settings_manager_interface.h>
#include <string>
#include <sys/types.h>  // For pid_t
#include <vector>

/**
 * @class LinuxSettingsManager
 * @brief Manages virtual displays on Linux using Xvfb.
 *
 * This class implements the SettingsManagerInterface to provide a
 * consistent way to manage virtual displays across different platforms.
 */
class LinuxSettingsManager: public display_device::SettingsManagerInterface {
public:
  LinuxSettingsManager();
  ~LinuxSettingsManager() override;

  /**
   * @brief Applies the given display settings.
   *
   * This will start an Xvfb process with the specified resolution.
   * @param config The display configuration to apply.
   * @return The result of the operation.
   */
  [[nodiscard]] ApplyResult applySettings(const display_device::SingleDisplayConfiguration &config) override;

  /**
   * @brief Reverts the display settings to their original state.
   *
   * This will terminate the Xvfb process.
   * @return The result of the operation.
   */
  [[nodiscard]] RevertResult revertSettings() override;

  /**
   * @brief Gets the display name for a given device ID.
   * @param device_id The device ID to map.
   * @return The display name.
   */
  [[nodiscard]] std::string getDisplayName(const std::string &device_id) const override;

  /**
   * @brief Enumerates the available display devices.
   * @return A list of available display devices.
   */
  [[nodiscard]] display_device::EnumeratedDeviceList enumAvailableDevices() const override;

  /**
   * @brief Resets the persistence of the display settings.
   * @return True if the persistence was reset, false otherwise.
   */
  [[nodiscard]] bool resetPersistence() override;

private:
  pid_t xfvb_pid = -1;  // The process ID of the Xvfb process
  std::string display_name = "";  // The name of the virtual display (e.g., ":1")
};
