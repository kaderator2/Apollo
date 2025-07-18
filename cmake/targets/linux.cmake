# linux specific target definitions
#
# Linux-specific target configurations
#

# Add all Linux-specific source files
target_sources(sunshine PRIVATE
  # Standard Linux platform files
  "${CMAKE_SOURCE_DIR}/src/platform/linux/audio.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/cuda.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/cuda.cu"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/graphics.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/kmsgrab.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/misc.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/publish.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/vaapi.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/wayland.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/wlgrab.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/x11grab.cpp"

  # Linux virtual display source files
  # "${CMAKE_SOURCE_DIR}/src/platform/linux/virtual_display.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/LinuxSettingsManager.cpp"
)

# --- Find and link libraries for virtual display ---
find_package(X11 REQUIRED)
# include(ExternalProject)

# Define the location of the evdi source from the submodule
# set(EVDI_SRC_DIR "${CMAKE_SOURCE_DIR}/third-party/evdi")

# Use ExternalProject_Add to build the evdi library using its own Makefile
# ExternalProject_Add(evdi_project
#   SOURCE_DIR      ${EVDI_SRC_DIR}/library
#   CONFIGURE_COMMAND ""
#   BUILD_COMMAND   make
#   BUILD_IN_SOURCE 1
#   INSTALL_COMMAND "" # No install step needed
# )

# message(STATUS "Configuring evdi library to be built from submodule.")

# Add the include directory from the submodule's library folder
# target_include_directories(sunshine PRIVATE "${EVDI_SRC_DIR}/library")

# Make the sunshine target depend on the evdi_project.
# add_dependencies(sunshine evdi_project)

# Add the full path to the library that will be created and Xrandr
# to the global list of libraries.
list(APPEND SUNSHINE_EXTERNAL_LIBRARIES
  # "${EVDI_SRC_DIR}/library/libevdi.so" # Link directly to the output file
  ${X11_Xrandr_LIBRARY}
)

