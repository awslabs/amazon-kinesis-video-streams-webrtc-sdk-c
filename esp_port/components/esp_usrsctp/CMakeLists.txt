set(usrsctp_root_dir ${CMAKE_CURRENT_SOURCE_DIR}/usrsctp/usrsctplib)

list(APPEND usrsctp_sources
	${usrsctp_root_dir}/netinet/sctp_asconf.c
	${usrsctp_root_dir}/netinet/sctp_auth.c
	${usrsctp_root_dir}/netinet/sctp_bsd_addr.c
	${usrsctp_root_dir}/netinet/sctp_callout.c
	${usrsctp_root_dir}/netinet/sctp_cc_functions.c
	${usrsctp_root_dir}/netinet/sctp_crc32.c
	${usrsctp_root_dir}/netinet/sctp_indata.c
	${usrsctp_root_dir}/netinet/sctp_input.c
	${usrsctp_root_dir}/netinet/sctp_output.c
	${usrsctp_root_dir}/netinet/sctp_pcb.c
	${usrsctp_root_dir}/netinet/sctp_peeloff.c
	${usrsctp_root_dir}/netinet/sctp_sha1.c
	${usrsctp_root_dir}/netinet/sctp_ss_functions.c
	${usrsctp_root_dir}/netinet/sctp_sysctl.c
	${usrsctp_root_dir}/netinet/sctp_timer.c
	${usrsctp_root_dir}/netinet/sctp_userspace.c
	${usrsctp_root_dir}/netinet/sctp_usrreq.c
	${usrsctp_root_dir}/netinet/sctputil.c
	${usrsctp_root_dir}/netinet6/sctp6_usrreq.c
	${usrsctp_root_dir}/user_environment.c
	${usrsctp_root_dir}/user_mbuf.c
	${usrsctp_root_dir}/user_recv_thread.c
	${usrsctp_root_dir}/user_socket.c
)

# Register our component
idf_component_register(
    SRCS
        ${usrsctp_sources}
    INCLUDE_DIRS
        ${usrsctp_root_dir}
        ${usrsctp_root_dir}/netinet
        ${usrsctp_root_dir}/netinet6
    REQUIRES
        lwip
        mbedtls
        esp_common
        pthread
    )

# Set SCTP options first
set(sctp_use_lwip ON)
set(sctp_use_rtos ON)
set(sctp_link_programs_static ON)

option(SCTP_USE_MBEDTLS_SHA1 "Build with mbedtls sha1 support." ON)

# Add definitions before including usrsctp
if(sctp_use_lwip)
    add_definitions(-DSCTP_USE_LWIP)
endif()

if(sctp_use_rtos)
    add_definitions(-DSCTP_USE_RTOS)
endif()

add_definitions(-DSCTP_DEBUG=1)
add_definitions(-D__Userspace__)
add_definitions(-DSCTP_SIMPLE_ALLOCATOR)
add_definitions(-DSCTP_PROCESS_LEVEL_LOCKS)
add_definitions(-DHAVE_SCONN_LEN)
add_definitions(-DSCTP_USE_LWIP)
add_definitions(-DSCTP_USE_RTOS)

if(NOT sctp_use_lwip)
	add_definitions(-D__native_client__)
endif()

#################################################
# CHECK FOR TYPES AND FUNCTIONS
#################################################

# check_include_files("sys/queue.h" have_sys_queue_h)
# if (have_sys_queue_h)
	add_definitions(-DHAVE_SYS_QUEUE_H)
# endif ()

# check_include_files("sys/socket.h;linux/if_addr.h" have_linux_if_addr_h)
# if (have_linux_if_addr_h)
	add_definitions(-DHAVE_LINUX_IF_ADDR_H)
# endif ()

# check_include_files("sys/socket.h;linux/rtnetlink.h" have_linux_rtnetlink_h)
# if (have_linux_rtnetlink_h)
# 	add_definitions(-DHAVE_LINUX_RTNETLINK_H)
# endif ()

# check_include_files("sys/types.h;netinet/in.h;netinet/ip.h;netinet/ip_icmp.h" have_netinet_ip_icmp_h)
# if (have_netinet_ip_icmp_h)
	# add_definitions(-DHAVE_NETINET_IP_ICMP_H)
# endif ()

# check_include_files("stdatomic.h" have_stdatomic_h)
# if (have_stdatomic_h)
	add_definitions(-DHAVE_STDATOMIC_H)
# endif ()

option(sctp_invariants "Add runtime checks" 0)
if (sctp_invariants)
	add_definitions(-DINVARIANTS)
endif ()

option(sctp_debug "Provide debug information" 1)
if (sctp_debug)
	add_definitions(-DSCTP_DEBUG)
endif ()

option(sctp_inet "Support IPv4" 1)
if (sctp_inet)
	add_definitions(-DINET)
endif ()

option(sctp_inet6 "Support IPv6" 0)
if (sctp_inet6)
	add_definitions(-DINET6)
endif ()

if(SCTP_USE_MBEDTLS_SHA1)
	add_definitions(-DSCTP_USE_MBEDTLS_SHA1)
	# if(NOT sctp_use_rtos)
	# 	find_package(MbedTLS REQUIRED)
	# endif()
endif()

# Add compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-format-truncation
    -Wno-address-of-packed-member
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-pointer-sign
    -Wno-implicit-function-declaration
    -Wno-incompatible-pointer-types
    -Wno-format
	-Wno-error=maybe-uninitialized
)
