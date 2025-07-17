#pragma once

#include <display_device/settings_manager_interface.h>
#include <memory>

class LinuxSettingsManager: public display_device::SettingsManagerInterface {
public:
  LinuxSettingsManager();
  ~LinuxSettingsManager() override;

  // --- Interface Implementation (Corrected) ---

  display_device::EnumeratedDeviceList enumAvailableDevices() const override;

  std::string getDisplayName(const std::string &device_id) const override;

  display_device::ApplyResult applySettings(const display_device::SingleDisplayConfiguration &config) override;

  display_device::RevertResult revertSettings() override;

  bool resetPersistence() override;

private:
  display_device::EnumeratedDeviceList m_original_config;
};
