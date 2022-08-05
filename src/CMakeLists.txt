add_llvm_library(LocalConcurrencyDetection MODULE
	LocalConcurrencyDetection.cpp

  PLUGIN_TOOL
  opt
  )

if(APPLE)
  # The magic line.
  # We're building a shared lib, tell OSX to resolve the symbol when actually
  # loading the library.
  # If we don't set this and manually add the appropriate LLVM libs, loading
  # the plugin will silently fail (!!!) because of duplicate symbols.
  set_target_properties(LocalConcurrencyDetection PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
    )
endif()

set(LOCALCONCURRENCYDETECTION_LIB ${CMAKE_CURRENT_BINARY_DIR}/LocalConcurrencyDetection${CMAKE_SHARED_LIBRARY_SUFFIX})
configure_file(lcd.in ${CMAKE_BINARY_DIR}/lcd @ONLY)
configure_file(lcdBC.in ${CMAKE_BINARY_DIR}/lcdBC @ONLY)

