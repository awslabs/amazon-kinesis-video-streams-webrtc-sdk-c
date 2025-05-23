idf_build_get_property(target IDF_TARGET)
set(srcs "mempool.c" )
set(priv_requires "")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ".")

