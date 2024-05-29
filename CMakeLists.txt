# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PhilipsVolumeReader")
find_package(GDCM CONFIG REQUIRED)
if(GDCM_FOUND)
  include(${GDCM_USE_FILE})
  set(GDCM_LIBRARIES gdcmcharls gdcmCommon gdcmDICT gdcmDSED gdcmMSFF)
endif()
find_package(VTK COMPONENTS CommonCore CommonDataModel IOImage IOXML)

# Include sub-projects.
add_subdirectory ("PhilipsVolumeReader")
add_subdirectory("Example")
