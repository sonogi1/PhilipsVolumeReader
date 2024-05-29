# CMakeList.txt : CMake project for PhilipsVolumeReader, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_library (vtkPhilipsVolumeReader SHARED "vtkPhilipsVolumeReader.cpp" "vtkPhilipsVolumeReader.h")
target_link_libraries(vtkPhilipsVolumeReader PUBLIC ${VTK_LIBRARIES} ${GDCM_LIBRARIES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vtkPhilipsVolumeReader PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
