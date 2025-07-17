# linux specific target definitions
# Add Linux-specific source files
target_sources(sunshine PRIVATE
  "${CMAKE_SOURCE_DIR}/src/platform/linux/audio.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/display.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/input.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/misc.cpp"
  "${CMAKE_SOURCE_DIR}/src/platform/linux/process.cpp"

  # NEW: Add your Linux virtual display source files
  "${CMAKE_SOURCE_DIR}/src/platform/linux/virtual_display.cpp"
  # Placeholder for the settings manager you will create next
  "${CMAKE_SOURCE_DIR}/src/platform/linux/settings_manager.cpp"
)

# --- Find and link libraries for virtual display ---
find_package(X11 REQUIRED)
find_library(EVDI_LIBRARY NAMES evdi REQUIRED)

if(EVDI_LIBRARY AND X11_Xrandr_FOUND)
  message(STATUS "Found EVDI library: ${EVDI_LIBRARY}")
  message(STATUS "Found Xrandr library: ${X11_Xrandr_LIBRARY}")

  # Link the required libraries to the main 'sunshine' target
  target_link_libraries(sunshine PRIVATE
    ${EVDI_LIBRARY}
    ${X11_Xrandr_LIBRARY}
  )
else()
  # This will stop the build if the required libraries aren't found
  message(FATAL_ERROR "Could not find required libraries: libevdi and/or libXrandr.")
endif()

