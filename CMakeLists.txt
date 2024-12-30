# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set(SFML_VERSION 3.0.0)
#set(IMGUI_VERSION 1.87)
set(IMGUI_VERSION 1.91.6)
set(IMGUI_SFML_VERSION 2.6)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("BobiFluidCollab" LANGUAGES CXX)

# Include sub-projects.
add_subdirectory ("BobiFluidCollab")
add_subdirectory("lib/glad")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
FetchContent_Declare(
  SFML
  URL "https://github.com/SFML/SFML/archive/${SFML_VERSION}.zip"
)

FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/v${IMGUI_VERSION}.zip"
)

FetchContent_Declare(
  imgui-sfml
#  URL "https://github.com/SFML/imgui-sfml/archive/v${IMGUI_SFML_VERSION}.zip"
	URL "https://github.com/SFML/imgui-sfml/archive/refs/heads/master.zip"
)

option(SFML_BUILD_AUDIO "Build audio" OFF)
option(SFML_BUILD_NETWORK "Build network" OFF)
option(SFML_BUILD_EXAMPLES "Build examples" OFF)
option(SFML_BUILD_TEST_SUITE "Build test suite" OFF)
option(SFML_BUILD_DOC "Build doc" OFF)

FetchContent_MakeAvailable(SFML)
FetchContent_MakeAvailable(imgui)

#sfml stuff
set(IMGUI_DIR ${imgui_SOURCE_DIR})
option(IMGUI_SFML_FIND_SFML "Use find_package to find SFML" OFF)
option(IMGUI_SFML_IMGUI_DEMO "Build imgui_demo.cpp" OFF)
FetchContent_MakeAvailable(imgui-sfml)