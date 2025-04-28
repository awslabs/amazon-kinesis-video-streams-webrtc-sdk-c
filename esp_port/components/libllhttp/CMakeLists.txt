set(llhttp_srcs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/llhttp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/include/llhttp.h
)

idf_component_register(
    SRCS ${llhttp_srcs}
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
)
