idf_component_register(SRCS "src/aws_signer_v4.c"
                            "src/auth.c"
                            "src/file_credential_provider.c"
                            "src/iot_credential_provider.c"
                            "src/static_credential_provider.c"
                            "src/http_iot_credentials.c"
                      INCLUDE_DIRS "include"
                      REQUIRES kvs_utils mbedtls esp_http_client)
