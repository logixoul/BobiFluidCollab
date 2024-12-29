file(GLOB SOURCES
    ./*.h
    ./*.cpp
)

add_executable (BobiFluidCollab ${SOURCES}  "precompiled.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET BobiFluidCollab PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

target_link_libraries(BobiFluidCollab PRIVATE SFML::Graphics)