project (libhal)

# option(HAL_PCAP_SUPPORTED "Ethernet requires pcap lib" OFF)
# option(HAL_TESTS "Hal tests" OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

FILE(GLOB libhal_h ./include/*.h)
FILE(GLOB libhal_src ./src/*.c)
add_library(libhal SHARED ${libhal_src} ${libhal_h})
if(WIN32)
    target_link_libraries(libhal PUBLIC ws2_32 iphlpapi winmm)
    if(HAL_PCAP_SUPPORTED)
        if(NOT DEFINED NPCAPLIB_DIR)
            set(NPCAPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../npcap")
        endif()
        include_directories(${NPCAPLIB_DIR}/Include)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_link_libraries(libhal PUBLIC 
                ${NPCAPLIB_DIR}/Lib/x64/wpcap.lib
                ${NPCAPLIB_DIR}/Lib/x64/packet.lib
            )
        else()
            target_link_libraries(libhal PUBLIC 
                ${NPCAPLIB_DIR}/Lib/wpcap.lib
                ${NPCAPLIB_DIR}/Lib/packet.lib
            )
        endif()
        target_compile_definitions(libhal PRIVATE HAL_PCAP_SUPPORTED)
    endif()
    target_compile_definitions(libhal PRIVATE 
        EXPORT_FUNCTIONS_FOR_DLL 
        _CRT_SECURE_NO_WARNINGS 
        _WINSOCK_DEPRECATED_NO_WARNING
    )
else()
    target_link_libraries(libhal PUBLIC pthread)
endif()

if(HAL_TESTS)
message("Hal tests are enabled")
include(tests/tests.cmake)
endif()
