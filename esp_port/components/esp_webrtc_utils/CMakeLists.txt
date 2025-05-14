set(srcs "src/message_utils.c")

set(requires )

idf_component_register(SRCS "${srcs}"
                       REQUIRES ${requires}
                       INCLUDE_DIRS "include")
