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
  "${CMAKE_SOURCE_DIR}/src/platform/linux/virtual_display.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/settings_manager.cpp"
)

# --- Find and link libraries for virtual display ---
find_package(X11 REQUIRED)
include(ExternalProject) # Include the module for building external projects
find_package(PkgConfig REQUIRED)
find_package(FFmpeg REQUIRED) # NEW: Find the FFmpeg package

# Define the location of the evdi source from the submodule
set(EVDI_SRC_DIR "${CMAKE_SOURCE_DIR}/third-party/evdi")
set(EVDI_BUILD_DIR "${CMAKE_BINARY_DIR}/third-party/evdi")

# Use ExternalProject_Add to build the evdi library using its own Makefile
ExternalProject_Add(evdi_project
  SOURCE_DIR    ${EVDI_SRC_DIR}
  CONFIGURE_COMMAND "" # No configure step needed for this Makefile
  # CORRECTED: Tell make to build in the 'library' subdirectory
  BUILD_COMMAND make -C "${EVDI_SRC_DIR}/library"
  INSTALL_COMMAND ""   # We will use the library from its build directory
  BINARY_DIR    ${EVDI_BUILD_DIR}
)

# Create an "imported" library target to represent the libevdi.so file
add_library(evdi_lib UNKNOWN IMPORTED)
set_target_properties(evdi_lib PROPERTIES
  IMPORTED_LOCATION "${EVDI_SRC_DIR}/library/libevdi.so"
)

# Make sure our imported library depends on the external project build
add_dependencies(evdi_lib evdi_project)

message(STATUS "Building evdi library from submodule using Makefile.")

# Add the include directory from the submodule's library folder
target_include_directories(sunshine PRIVATE "${EVDI_SRC_DIR}/library")

# Add our newly built library and Xrandr to the global list of libraries
list(APPEND SUNSHINE_EXTERNAL_LIBRARIES
  evdi_lib # Link against our imported library target
  ${X11_Xrandr_LIBRARY}
)
