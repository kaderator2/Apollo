#include "LinuxSettingsManager.h"

#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

LinuxSettingsManager::LinuxSettingsManager() {
  // Constructor
}

LinuxSettingsManager::~LinuxSettingsManager() {
  // Make sure to clean up the Xvfb process when the manager is destroyed.
  if (xfvb_pid != -1) {
    revertSettings();
  }
}

display_device::SettingsManagerInterface::ApplyResult LinuxSettingsManager::applySettings(const display_device::SingleDisplayConfiguration &config) {
  // If a virtual display is already running, destroy it before creating a new one.
  if (xfvb_pid != -1) {
    revertSettings();
  }

  // Generate a new display name
  // NOTE: This is a simple approach. A more robust solution would check for available display numbers.
  static int display_num = 1;
  display_name = ":" + std::to_string(display_num++);

  // Fork the process to start Xvfb
  xfvb_pid = fork();

  if (xfvb_pid == -1) {
    // Fork failed
    std::cerr << "Failed to fork process for Xvfb" << std::endl;
    return ApplyResult::Failed;  // NOTE: This should map to a more appropriate error in the enum
  } else if (xfvb_pid == 0) {
    // Child process
    std::string resolution = "1920x1080x24";  // Default
    if (config.m_resolution) {
      resolution = std::to_string(config.m_resolution->m_width) + "x" + std::to_string(config.m_resolution->m_height) + "x24";
    }

    execlp("Xvfb", "Xvfb", display_name.c_str(), "-screen", "0", resolution.c_str(), (char *) NULL);

    // If execlp returns, it must have failed
    std::cerr << "Failed to execute Xvfb" << std::endl;
    exit(1);
  } else {
    // Parent process
    // A small delay to allow Xvfb to initialize
    sleep(1);

    // Set the DISPLAY environment variable for the Sunshine process
    setenv("DISPLAY", display_name.c_str(), 1);
    std::cout << "Started Xvfb with PID " << xfvb_pid << " on display " << display_name << std::endl;
    return ApplyResult::Ok;
  }
}

display_device::SettingsManagerInterface::RevertResult LinuxSettingsManager::revertSettings() {
  if (xfvb_pid != -1) {
    // Kill the Xvfb process
    if (kill(xfvb_pid, SIGTERM) == 0) {
      // Wait for the process to exit
      int status;
      waitpid(xfvb_pid, &status, 0);
      std::cout << "Stopped Xvfb with PID " << xfvb_pid << std::endl;
      xfvb_pid = -1;
      display_name = "";
      // Unset the DISPLAY environment variable
      unsetenv("DISPLAY");
      return RevertResult::Ok;
    } else {
      std::cerr << "Failed to kill Xvfb process with PID " << xfvb_pid << std::endl;
      // This could be considered a failure to revert
      return RevertResult::SwitchingTopologyFailed;
    }
  }
  // Nothing to revert
  return RevertResult::Ok;
}

std::string LinuxSettingsManager::getDisplayName(const std::string &device_id) const {
  // In our simple case, the device_id could be the display name itself.
  // A more advanced implementation could query X11 for more details.
  if (device_id == display_name) {
    return display_name;
  }
  return "";
}

display_device::EnumeratedDeviceList LinuxSettingsManager::enumAvailableDevices() const {
  // This would need to query the X server for a list of displays.
  // For now, if our virtual display is active, we can report it.
  display_device::EnumeratedDeviceList devices;
  if (xfvb_pid != -1) {
    display_device::EnumeratedDevice device;
    device.m_device_id = display_name;
    device.m_display_name = display_name;
    device.m_friendly_name = "Xvfb Virtual Display";
    device.m_is_active = true;
    devices.push_back(device);
  }
  return devices;
}

bool LinuxSettingsManager::resetPersistence() {
  // This concept doesn't directly map to our transient Xvfb implementation,
  // so we can consider it a success.
  return true;
}
