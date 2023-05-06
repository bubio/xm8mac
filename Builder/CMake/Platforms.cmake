if(WIN32)
  include(platforms/windows)
endif()

if(ANDROID)
  include(platforms/android)
endif()

if(IOS)
  include(platforms/ios)
endif()

if(UWP_LIB)
  include(platforms/uwp_lib)
endif()