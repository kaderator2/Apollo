#pragma once

#include <display_device/settings_manager_interface.h>
#include <memory>

class LinuxSettingsManager: public display_device::SettingsManagerInterface {
public:
  LinuxSettingsManager();
  ~LinuxSettingsManager() override;

  // --- Interface Implementation (Corrected to perfectly match the base class) ---

  display_device::EnumeratedDeviceList enumAvailableDevices() const override;

  std::string getDisplayName(const std::string &device_id) const override;

  ApplyResult applySettings(const SingleDisplayConfiguration &config) override;
  RevertResult revertSettings() override;
  bool resetPersistence() override;

private:
  std::vector<display_device::SingleDisplayConfiguration> m_original_config;
};
