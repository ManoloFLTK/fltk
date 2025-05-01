#
# Example CMakeLists.txt file that uses an FLTK test program as an example of FLTK client program
# and allows to build it.
# Under macOS, the resulting app bundle contains FLTK libraries as frameworks.
#

cmake_minimum_required(VERSION 3.15)

project(fltk_client)

set(FLTK_DIR "${PROJECT_SOURCE_DIR}/../build" CACHE FILEPATH "FLTK build directory")

find_package(FLTK 1.5 CONFIG REQUIRED)

add_executable       (fltk_client WIN32 MACOSX_BUNDLE ${PROJECT_SOURCE_DIR}/../test/pixmap_browser.cxx)
target_link_libraries(fltk_client PRIVATE fltk::images-shared)

if(APPLE)
  include(${FLTK_DIR}/FLTK-Functions.cmake)
  fltk_cp_frameworks_to_bundle(${PROJECT_NAME} ${FLTK_DIR} ${PROJECT_BINARY_DIR})
endif(APPLE)
