#
# Example CMakeLists.txt file that uses an FLTK test program as an example of FLTK client program
# and allows to build it.
# Under macOS, the resulting app bundle contains FLTK libraries as frameworks.
#

cmake_minimum_required(VERSION 3.15)

project(fltk_client)

set(FLTK_DIR "${PROJECT_SOURCE_DIR}/../build" CACHE FILEPATH "FLTK build directory")

find_package(FLTK CONFIG REQUIRED)

if(APPLE AND NOT FLTK_BACKEND_X11 )
  set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif(APPLE AND NOT FLTK_BACKEND_X11 )

add_executable       (fltk_client WIN32 MACOSX_BUNDLE ${PROJECT_SOURCE_DIR}/../test/pixmap_browser.cxx)
target_link_libraries(fltk_client PRIVATE fltk::images-shared)

get_property(FLTK_TARGETS TARGET ${PROJECT_NAME} PROPERTY LINK_LIBRARIES)
if(APPLE AND NOT FLTK_BACKEND_X11 AND FLTK_TARGETS MATCHES "fltk::[a-z]*-shared")
  #set FLTK_USED_LIBS to list of necessary FLTK frameworks
  set(FLTK_USED_LIBS fltk)
  if (FLTK_TARGETS MATCHES fltk::images-shared)
    list(PREPEND FLTK_USED_LIBS fltk_images fltk_png fltk_jpeg fltk_z)
  endif()
  if (FLTK_TARGETS MATCHES fltk::gl-shared)
    list(PREPEND FLTK_USED_LIBS fltk_gl)
  endif()
 
  #declare that dylibs are to be found in the Frameworks section of the bundle
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,@loader_path/../Frameworks")

  if(CMAKE_GENERATOR STREQUAL Xcode)
    set(PROJECT_PREFIX $<CONFIG>/)
  else()
    set(PROJECT_PREFIX )
  endif(CMAKE_GENERATOR STREQUAL Xcode)
  set(BUNDLE_FRAMEWORK_PATH ${PROJECT_BINARY_DIR}/${PROJECT_PREFIX}${PROJECT_NAME}.app/Contents/Frameworks)
  #create the Frameworks directory inside bundle
  add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK 
    COMMAND mkdir ARGS -p ${BUNDLE_FRAMEWORK_PATH})
  #copy needed frameworks to bundle from FLTK build dir
  foreach(ELT ${FLTK_USED_LIBS})
    add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK 
      COMMAND cp ARGS -R ${FLTK_DIR}/lib/${PROJECT_PREFIX}/${ELT}.framework ${BUNDLE_FRAMEWORK_PATH})
  endforeach()       
endif()
