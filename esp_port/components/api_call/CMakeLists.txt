idf_component_register(SRCS "http_helper.c"
                            "http_api.c"
                            "http_api_rsp.c"
                            "netio.c"
                      INCLUDE_DIRS "."
                      REQUIRES kvs_utils mbedtls credential esp_http_client)
