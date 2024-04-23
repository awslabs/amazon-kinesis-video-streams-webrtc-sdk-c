cmake_minimum_required(VERSION 3.6.3)

project(KinesisVideoWebRTCClientSamples LANGUAGES C)

#set(OPEN_SRC_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/open-source" CACHE PATH "Libraries will be downloaded and build in this directory.")

message("OPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX}")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST gstreamer-1.0)
if(GST_FOUND)
  if (WIN32)
    if(NOT DEFINED PKG_CONFIG_EXECUTABLE)
      if(EXISTS "C:\\gstreamer\\1.0\\x86_64\\bin\\pkg-config.exe")
        set(PKG_CONFIG_EXECUTABLE "C:\\gstreamer\\1.0\\x86_64\\bin\\pkg-config.exe")
      endif()
    elseif(DEFINED PKG_CONFIG_EXECUTABLE)
      message(STATUS "Gstreamer pkg-config path set to ${PKG_CONFIG_EXECUTABLE}")
    else()
      message(FATAL_ERROR "Gstreamer not found in default path. Set the appropriate path with -DPKG_CONFIG_EXECUTABLE=<path>")
    endif()
  endif()
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/libffi/lib/pkgconfig")
    find_library(Intl "intl" REQUIRED PATHS "/usr/local/opt/gettext/lib")
    link_directories("/usr/local/opt/gettext/lib")
  endif()

  pkg_check_modules(GLIB2 REQUIRED glib-2.0)
  pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
  pkg_check_modules(GOBJ2 REQUIRED gobject-2.0)
  message("gstreamer found. Will build gstreamer samples")

  set(OPEN_SRC_INCLUDE_DIRS
      ${OPEN_SRC_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${GST_INCLUDE_DIRS}
      ${GST_APP_INCLUDE_DIRS} ${GOBJ2_INCLUDE_DIRS})

  set(GST_SAMPLE_LIBRARIES ${GLIB2_LIBRARIES} ${GST_LIBRARIES}
                           ${GST_APP_LIBRARIES} ${GOBJ2_LIBRARIES} ${Intl})

  link_directories(${GLIB2_LIBRARY_DIRS})
  link_directories(${GST_LIBRARY_DIRS})
  link_directories(${GST_APP_LIBRARY_DIRS})
  link_directories(${GOBJ2_LIBRARY_DIRS})
else()
  message("gstreamer not found. Will not build gstreamer samples")
endif()

include_directories(${OPEN_SRC_INSTALL_PREFIX}/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})
link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib)

# copy sample frames to this subproject build folder, in case developer runs sample program with command `kvsWebrtcClientMaster` from `build/samples` dir.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/opusSampleFrames" DESTINATION .)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/h264SampleFrames" DESTINATION .)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/aacSampleFrames" DESTINATION .)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/h265SampleFrames" DESTINATION .)

add_executable(
  kvsWebrtcClientMaster
  Common.c
  kvsWebRTCClientMaster.c)
target_link_libraries(kvsWebrtcClientMaster kvsWebrtcClient kvsWebrtcSignalingClient ${EXTRA_DEPS} kvsCommonLws kvspicUtils websockets kvssdp kvsstun kvsrtp)

add_executable(
  kvsWebrtcClientViewer
  Common.c
  kvsWebRTCClientViewer.c)
target_link_libraries(kvsWebrtcClientViewer kvsWebrtcClient kvsWebrtcSignalingClient ${EXTRA_DEPS} kvsCommonLws kvspicUtils websockets kvssdp kvsstun kvsrtp)

add_executable(
        discoverNatBehavior
        discoverNatBehavior.c)
target_link_libraries(discoverNatBehavior kvsWebrtcClient ${EXTRA_DEPS} kvssdp kvsstun kvsrtp)

if(GST_FOUND)
  add_executable(
    kvsWebrtcClientMasterGstSample
    Common.c
    GstAudioVideoReceiver.c
    kvsWebRTCClientMasterGstSample.c
  )
  target_link_libraries(kvsWebrtcClientMasterGstSample kvsWebrtcClient kvsWebrtcSignalingClient ${EXTRA_DEPS} ${GST_SAMPLE_LIBRARIES} kvsCommonLws kvspicUtils websockets kvssdp kvsstun kvsrtp)

  install(TARGETS kvsWebrtcClientMasterGstSample
    RUNTIME DESTINATION bin
  )

  add_executable(
    kvsWebrtcClientViewerGstSample
    Common.c
    GstAudioVideoReceiver.c
    kvsWebRTCClientViewerGstSample.c
  )
  target_link_libraries(kvsWebrtcClientViewerGstSample kvsWebrtcClient kvsWebrtcSignalingClient ${EXTRA_DEPS} ${GST_SAMPLE_LIBRARIES} kvsCommonLws kvspicUtils websockets kvssdp kvsrtp)

  install(TARGETS kvsWebrtcClientViewerGstSample
    RUNTIME DESTINATION bin
  )
endif()

install(TARGETS kvsWebrtcClientMaster kvsWebrtcClientViewer discoverNatBehavior
  RUNTIME DESTINATION bin
)

