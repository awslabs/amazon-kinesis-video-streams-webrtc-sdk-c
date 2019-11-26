cmake_minimum_required(VERSION 2.8)

project (WebRTCClientTest)

if(BP_BUILD_LIB)
    set(CMAKE_CXX_STANDARD 11)
    file (GLOB WebRTCClientTestSources *.cpp)

    set(INCLUDES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../src/include/")
    set(INCLUDES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../src/")
    set(INCLUDES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/")

    include_directories(${INCLUDES_PATH})
    include_directories(${BP_BUILD_INCLUDE_DIRS})
    link_directories(${BP_BUILD_LIB})

    add_executable(${PROJECT_NAME} ${WebRTCClientTestSources})
    target_link_libraries(${PROJECT_NAME} KinesisVideoWebRTCClient ${BP_TEST_BUILD_LINK_LIBS})
    target_link_libraries(${PROJECT_NAME} ${LIBSRTP_LIBRARIES} gtest)

    add_test(${PROJECT_NAME} ${PROJECT_NAME})
else()

    set(CMAKE_CXX_STANDARD 11)
    set(KINESIS_VIDEO_WebRTCClient_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)
    set(EXE_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS})

    if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(EXE_LIBRARIES ${EXE_LIBRARIES} rt)
    endif()

    set(GTEST_ROOT $ENV{KVS_GTEST_ROOT})
    find_package(GTest REQUIRED)

    file(GLOB WEBRTC_CLIENT_TEST_SOURCE_FILES
        "*.cpp"
    )

    include_directories(${KINESIS_VIDEO_WEBRTC_CLIENT_SRC})

    add_executable(webrtc_client_test ${WEBRTC_CLIENT_TEST_SOURCE_FILES})
    target_link_libraries(webrtc_client_test
            kvsWebrtcClient
            kvsWebrtcSignalingClient
            GTest::GTest
            GTest::Main
            ${EXE_LIBRARIES}
            ${Jsmn})
endif()
