#pragma once

#include <display_device/settings_manager_interface.h>
#include <memory>

// Forward declaration for a potential helper class
class LinuxDisplayDevice;

class LinuxSettingsManager: public display_device::SettingsManagerInterface {
public:
  LinuxSettingsManager();
  ~LinuxSettingsManager() override;

  // --- Interface Implementation ---

  display_device::EnumeratedDeviceList enumAvailableDevices() override;

  std::string getDisplayName(const std::string &device_id) override;

  display_device::ApplyResult applySettings(const display_device::SingleDisplayConfiguration &config) override;

  display_device::RevertResult revertSettings() override;

  bool resetPersistence() override;

private:
  // You can add private members here to store state,
  // such as a pointer to a display device helper.
  std::unique_ptr<LinuxDisplayDevice> m_display_device;

  // To store the state of the displays before we change them
  display_device::DisplayConfiguration m_original_config;
};
