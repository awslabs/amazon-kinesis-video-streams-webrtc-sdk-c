idf_component_register(SRCS "aws_signer_v4.c"
                            "auth.c"
                            "file_credential_provider.c"
                            "iot_credential_provider.c"
                            "static_credential_provider.c"
                      INCLUDE_DIRS "."
                      REQUIRES kvs_utils mbedtls)
