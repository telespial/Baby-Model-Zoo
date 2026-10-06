cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(
    frdm_mcxc162_temperature
    LANGUAGES C CXX ASM
    CUSTOM_LINKER TRUE
    PROJECT_BOARD_PORT_PATH
        ${board_root}/${board}/driver_examples/lpi2c/master_read_sensor_p3t1755
)

include(${SdkRootDirPath}/CMakeLists.txt)

mcux_add_source(SOURCES main.c flash_logger.c ../ai/baby_temp_ae.c ../ai/baby_temp_service.c ../ai/baby_temp_predictor.c ../ai/baby_temp_persistence.c)

mcux_add_include(INCLUDES . ../ai)

remove_defined_linker()
mcux_add_armgcc_linker_script(LINKER MCXC162_flash_logger.ld)

# Keep the full recorder within the first 48 KB, leaving two complete 8 KB
# sectors exclusively for the wear-levelled FIFO.
mcux_add_armgcc_configuration(CC "-Os")

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES
        ${board_root}/${board}/driver_examples/lpi2c/master_read_sensor_p3t1755
        ${board_root}/${board}/driver_examples/lpi2c/master_read_sensor_p3t1755/${core_id}
)

mcux_add_macro(
    CC "-DPRINTF_ADVANCED_ENABLE=1\
        -DPRINTF_FLOAT_ENABLE=1"
)

include(
    ${SdkRootDirPath}/${board_root}/${board}/driver_examples/lpi2c/master_read_sensor_p3t1755/reconfig.cmake
    OPTIONAL
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
