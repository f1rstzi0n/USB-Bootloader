cmake_minimum_required(VERSION 3.20)
project(boot_host_tests C)
set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -Wextra -Werror)
enable_testing()
set(BOOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../App/firmware)
set(CRYPTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/Crypto)
set(MBEDTLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/MbedTLS)
set(FW_CRYPTO_SOURCES
    ${MBEDTLS_DIR}/library/asn1parse.c ${MBEDTLS_DIR}/library/asn1write.c
    ${MBEDTLS_DIR}/library/bignum.c ${MBEDTLS_DIR}/library/bignum_core.c
    ${MBEDTLS_DIR}/library/bignum_mod.c ${MBEDTLS_DIR}/library/bignum_mod_raw.c
    ${MBEDTLS_DIR}/library/constant_time.c ${MBEDTLS_DIR}/library/ecp.c
    ${MBEDTLS_DIR}/library/ecp_curves.c ${MBEDTLS_DIR}/library/ecdsa.c
    ${MBEDTLS_DIR}/library/sha256.c ${MBEDTLS_DIR}/library/platform.c
    ${MBEDTLS_DIR}/library/platform_util.c ${MBEDTLS_DIR}/library/memory_buffer_alloc.c)
add_library(fw_crypto STATIC ${FW_CRYPTO_SOURCES})
target_include_directories(fw_crypto PUBLIC ${MBEDTLS_DIR}/include ${BOOT_DIR} ${CRYPTO_DIR}
    PRIVATE ${MBEDTLS_DIR}/library)
target_compile_definitions(fw_crypto PUBLIC MBEDTLS_CONFIG_FILE="fw_mbedtls_config.h")
if(NOT MSVC)
    target_compile_options(fw_crypto PRIVATE -ffunction-sections -fdata-sections)
endif()
add_executable(test_boot_update test_boot_update.c ${BOOT_DIR}/fw_update.c ${BOOT_DIR}/fw_image.c ${CRYPTO_DIR}/fw_crypto.c)
target_link_libraries(test_boot_update PRIVATE fw_crypto)
target_compile_definitions(test_boot_update PRIVATE FW_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/firmware")
add_test(NAME boot_update COMMAND test_boot_update)
add_executable(test_boot_usb test_boot_usb.c ${BOOT_DIR}/../usb/usb_descriptors.c)
target_include_directories(test_boot_usb PRIVATE ${BOOT_DIR}/../usb ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/TinyUSB/src)
add_test(NAME boot_usb COMMAND test_boot_usb)
