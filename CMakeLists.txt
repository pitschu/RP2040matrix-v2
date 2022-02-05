
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}../../pico-sdk")
# set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}../../FreeRTOS-Kernel-SMP")
set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/../FreeRTOSv202112.00/FreeRTOS/Source")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)
 
project(RP2040matrix_Proj C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(RP2040matrix
	RP2040matrixDemo.c
	hub75.c ps_hub75_128.pio ps_hub75_64.pio
	gol.c pong.c
	LEDmx.c)

pico_generate_pio_header(RP2040matrix ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_128.pio)
pico_generate_pio_header(RP2040matrix ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_64.pio)


pico_enable_stdio_uart(RP2040matrix 0)
pico_enable_stdio_usb(RP2040matrix 1)

target_sources(RP2040matrix PRIVATE RP2040matrixDemo.c)

target_include_directories(RP2040matrix PRIVATE include/
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../Common/include)

target_compile_definitions(RP2040matrix PRIVATE
	PICO_DEFAULT_UART_TX_PIN=28
	PICO_DEFAULT_UART_RX_PIN=29
        PCB_LAYOUT_V1=1
        HUB75_SIZE=4040         # 4040 = 64x64, other value is 8080 for 128x128
)

pico_add_extra_outputs(RP2040matrix)

target_link_libraries(RP2040matrix PRIVATE 
        hardware_pio 
        hardware_dma 
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap1
        pico_stdlib
        pico_multicore)

add_custom_command(TARGET RP2040matrix
        POST_BUILD
        COMMAND arm-none-eabi-size -B RP2040matrix.elf
        )


#########################################################################
add_executable(RP2040matrix_64_BCM
	RP2040matrixDemo.c
	hub75_BCM.c ps_hub75_128_BCM.pio ps_hub75_64_BCM.pio
	gol.c pong.c
	LEDmx.c)

pico_generate_pio_header(RP2040matrix_64_BCM ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_128_BCM.pio)
pico_generate_pio_header(RP2040matrix_64_BCM ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_64_BCM.pio)
pico_enable_stdio_uart(RP2040matrix_64_BCM 0)
pico_enable_stdio_usb(RP2040matrix_64_BCM 1)

target_sources(RP2040matrix_64_BCM PRIVATE RP2040matrixDemo.c)
target_include_directories(RP2040matrix_64_BCM PRIVATE include/
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../Common/include)
target_compile_definitions(RP2040matrix_64_BCM PRIVATE
	PICO_DEFAULT_UART_TX_PIN=28
	PICO_DEFAULT_UART_RX_PIN=29
        HUB75_BCM=1
        PCB_LAYOUT_V2=1
        HUB75_SIZE=4040         # 4040 = 64x64, other value is 8080 for 128x128
)
pico_add_extra_outputs(RP2040matrix_64_BCM)
target_link_libraries(RP2040matrix_64_BCM PRIVATE 
        hardware_pio 
        hardware_dma 
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap1
        pico_stdlib
        pico_multicore)
add_custom_command(TARGET RP2040matrix_64_BCM
        POST_BUILD
        COMMAND arm-none-eabi-size -B RP2040matrix_64_BCM.elf
        )

#########################################################################
add_executable(RP2040matrix_128_BCM
	RP2040matrixDemo.c
	hub75_BCM.c ps_hub75_128_BCM.pio ps_hub75_64_BCM.pio
	gol.c pong.c
	LEDmx.c)

pico_generate_pio_header(RP2040matrix_128_BCM ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_128_BCM.pio)
pico_generate_pio_header(RP2040matrix_128_BCM ${CMAKE_CURRENT_LIST_DIR}/ps_hub75_64_BCM.pio)
pico_enable_stdio_uart(RP2040matrix_128_BCM 0)
pico_enable_stdio_usb(RP2040matrix_128_BCM 1)

target_sources(RP2040matrix_128_BCM PRIVATE RP2040matrixDemo.c)
target_include_directories(RP2040matrix_128_BCM PRIVATE include/
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../Common/include)
target_compile_definitions(RP2040matrix_128_BCM PRIVATE
	PICO_DEFAULT_UART_TX_PIN=28
	PICO_DEFAULT_UART_RX_PIN=29
        HUB75_BCM=1
        PCB_LAYOUT_V2=1
        HUB75_SIZE=8080         # 4040 = 64x64, other value is 8080 for 128x128
)
pico_add_extra_outputs(RP2040matrix_128_BCM)
target_link_libraries(RP2040matrix_128_BCM PRIVATE 
        hardware_pio 
        hardware_dma 
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap1
        pico_stdlib
        pico_multicore)
add_custom_command(TARGET RP2040matrix_128_BCM
        POST_BUILD
        COMMAND arm-none-eabi-size -B RP2040matrix_128_BCM.elf
        )
