idf_component_register(SRCS "http_iot_credentials.c"
                       INCLUDE_DIRS "."
                       REQUIRES kvs_utils esp_http_client mbedtls)
