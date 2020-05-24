cmake_minimum_required(VERSION 2.8)
# VitaPad
set(PROJECT VitaPad)

project(${PROJECT})

file(GLOB SRC_FLATBUFFERS ../common/*.fbs)

foreach(FILE ${SRC_FLATBUFFERS})
  get_filename_component(BASE_NAME ${FILE} NAME_WE)
  set(FL_NAME "../include/${BASE_NAME}.fbs.hpp")
  list(APPEND FLATBUFFERS_OUT ${FL_NAME})
  add_custom_command(
    OUTPUT ${FL_NAME}
    COMMAND
      "flatc" ARGS --cpp --scoped-enums ARGS --gen-mutable ARGS
      --filename-suffix ".fbs" ARGS --filename-ext "hpp" ARGS -o ../include ARGS
      ${FILE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${FILE})
endforeach()
include_directories(../include)

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PC_APPINDICATOR REQUIRED appindicator-0.1)
  pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
  find_path(
    APPINDICATOR_INCLUDE_DIR
    NAMES libappindicator/app-indicator.h
    HINTS ${PC_APPINDICATOR_INCLUDEDIR} ${PC_APPINDICATOR_INCLUDE_DIRS}
    PATH_SUFFIXES libappindicator-0.1)
  find_library(APPINDICATOR_LIBRARY NAMES appindicator-0.1)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    APPINDICATOR DEFAULT_MSG APPINDICATOR_LIBRARY APPINDICATOR_INCLUDE_DIR)
  if(APPINDICATOR_FOUND)
    set(APPINDICATOR_LIBRARIES ${APPINDICATOR_LIBRARY})
    set(APPINDICATOR_INCLUDE_DIRS ${APPINDICATOR_INCLUDE_DIR})
  endif()
  mark_as_advanced(APPINDICATOR_INCLUDE_DIR APPINDICATOR_LIBRARY)

  link_directories(${GTK2_LIBRARY_DIRS} ${APPINDICATOR_LIBRARY})

  add_executable(${PROJECT} src/main.cpp src/ctrl.cpp src/sock_data.cpp
                            src/uinput.c ${FLATBUFFERS_OUT})

  include_directories(/usr/include/libevdev-1.0/ ${GTK2_INCLUDE_DIRS} ${APPINDICATOR_INCLUDE_DIR})

  target_link_libraries(${PROJECT} evdev pthread ${GTK2_LIBRARIES} ${APPINDICATOR_LIBRARY})

elseif(WIN32)

  add_custom_command(
    OUTPUT vendor/lib/release/x64/ViGEmClient.lib
    COMMAND vendor/build.ps1 -configuration Release_LIB|x64
    WORKING_DIRECTORY vendor/)
  add_executable(${PROJECT} src/main.cpp src/ctrl.cpp src/sock_data.cpp
                            src/vigem.c ${FLATBUFFERS_OUT})
  target_link_libraries(${PROJECT} vendor/lib/release/x64/ViGEmClient.lib)

endif(UNIX)
