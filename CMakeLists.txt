# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
# Board selection via the standard pico-sdk mechanism (Phase 2b). The board
# header in boards/<PICO_BOARD>.h supplies all pins and includes the base
# chip header (pico2.h / pico.h).
set(PICO_PLATFORM "rp2350" CACHE STRING "Chip type")
set(PICO_BOARD murmulator CACHE STRING "Board: murmulator | murmulator2 | olimex-pico-pc | waveshare_rp2040_pizero | waveshare_rp2350_pizero")
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
include(pico_sdk_import.cmake)

project(pico-mac C CXX ASM)

option(I2S "Enable I2S sound" OFF)
option(I2S_CS4334 "Enable I2S CS4334 sound" OFF)
option(VGA "Enable VGA" OFF)
option(TFT "Enable TFT display" OFF)
option(ILI9341 "Enable TFT ILI9341 display" OFF)
option(HDMI_DVI "Enable HDMI display" ON)
option(TV "Enable TV composite output" OFF)
option(SOFTTV "Enable TV soft composite output" OFF)

# BENCH_EMU:    build an instrumented image that reports emulation speed over
#               UART once per second (see docs/PHASE0.md). PS/2 keyboard is
#               unavailable in this build (UART reuses GP0/GP1); USB HID works.
# DEV_SMALL_RAM: develop against a small Mac RAM so headroom is not a concern.
#               ON  -> 208 KB (uses the shipped umac-rom-208-vga.h).
#               OFF -> 464 KB (release-sized, uses umac-rom-464-vga.h).
#               128 KB is the true stock size but needs a generated
#               umac-rom-128-vga.h (not shipped); see docs/PHASE0.md.
option(BENCH_EMU "Enable on-screen emulation-speed benchmark HUD" ON)
option(DEV_SMALL_RAM "Use small (208K) Mac RAM for development" ON)

if(DEV_SMALL_RAM)
    set(UMAC_MEMSIZE_VAL 208)
else()
    set(UMAC_MEMSIZE_VAL 464)
endif()

set (PORT_VERSION "0.1.0")

#set(HDMI_DVI ON)

#set(I2S ON)
###set(TV ON)
###set(SOFTTV ON)
###set(TFT ON)
###set(ILI9341 ON)

#set (MURM2 ON)
#set (MONSTR ON)
set(KBDUSB ON)

#set(HDMI ON)
###set(FLASH_SIZE 4096)
set(FLASH_SIZE 2048)
set(FLASH_SIZE_GREEN ON)
###set(CPU_MHZ 408)
set(CPU_MHZ 378)
# Phase 1: core voltage selectable (lower to reduce heat/stress at cost of max clock)
set(VREG_VOLTAGE "VREG_VOLTAGE_1_50" CACHE STRING "Core voltage enum, e.g. VREG_VOLTAGE_1_30")
#set(CPU_MHZ 366)



IF(MURM2)
    SET(BUILD_NAME "m2p2-${PROJECT_NAME}-${CPU_MHZ}")
ELSE()
    SET(BUILD_NAME "m1p2-${PROJECT_NAME}-${CPU_MHZ}")
ENDIF()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}  -funroll-loops -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -O2")

pico_sdk_init()

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

add_subdirectory(drivers/audio)
add_subdirectory(drivers/picomp3lib)

add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/vga-nextgen)
add_subdirectory(drivers/st7789)
add_subdirectory(drivers/hdmi)
# Phase 3a: PicoDVI engine (libdvi) imported from emu80@760330a. INTERFACE
# library: nothing is compiled until a target links it. The Mac 1bpp HDMI-DVI
# driver that uses it (and links it) is added in Phase 3b.
add_subdirectory(drivers/libdvi)
add_subdirectory(drivers/hdmi-dvi)
add_subdirectory(drivers/tv)
add_subdirectory(drivers/tv-software)
add_subdirectory(drivers/graphics)
add_subdirectory(drivers/psram)

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC
 "src/*.cpp"
 "src/*.c"
 "external/umac/src/main.c"
 "external/umac/src/disc.c"
 "external/umac/src/scc.c"
 "external/umac/src/via.c"
 "external/umac/external/Musashi/m68kcpu.c"
 "external/umac/external/Musashi/m68kdasm.c"
 "external/umac/external/Musashi/m68kops.c"
)

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})

# Cross-TU inlining is especially valuable for Musashi: every emulated memory
# access otherwise crosses from m68kops.c into the umac address decoder.
#include(CheckIPOSupported)
#check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR LANGUAGES C CXX)
#if(IPO_SUPPORTED)
#    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#else()
#    message(WARNING "IPO/LTO is not supported: ${IPO_ERROR}")
#endif()

# Keep the emulator core aggressive without increasing every driver.
set_source_files_properties(
    external/umac/src/main.c
    external/umac/src/via.c
    external/umac/external/Musashi/m68kcpu.c
    external/umac/external/Musashi/m68kops.c
    PROPERTIES COMPILE_OPTIONS "-O3"
)

pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

pico_set_program_name(pico-mac "Pico Mac port for Murmulator - Macintosh 128-512K emulator by DnCraptor")
pico_set_program_version(pico-mac PORT_VERSION)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    external/umac/include
    external/umac/external/Musashi
)

IF(NOT I2S)
#    SET(BUILD_NAME "${BUILD_NAME}-PWM")
    target_compile_definitions(${PROJECT_NAME} PRIVATE AUDIO_PWM_PIN=26)
ELSEIF(I2S_CS4334)
    target_compile_definitions(${PROJECT_NAME} PRIVATE I2S_CS4334)
    SET(BUILD_NAME "${BUILD_NAME}-CS4334")
ELSE()
    SET(BUILD_NAME "${BUILD_NAME}-I2S")
    target_compile_definitions(${PROJECT_NAME} PRIVATE I2S_SOUND=1)
ENDIF()

IF(TFT)
    target_link_libraries(${PROJECT_NAME} PRIVATE st7789)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TFT)
    IF(ILI9341)
        SET(BUILD_NAME "${BUILD_NAME}-ILI9341-${PORT_VERSION}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE ILI9341)
    ELSE()
        SET(BUILD_NAME "${BUILD_NAME}-ST7789-${PORT_VERSION}")
    ENDIF()
ELSEIF(HDMI)
    target_link_libraries(${PROJECT_NAME} PRIVATE hdmi)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HDMI)
    SET(BUILD_NAME "${BUILD_NAME}-HDMI-${PORT_VERSION}")
ELSEIF(TV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TV)
    target_link_libraries(${PROJECT_NAME} PRIVATE tv)
    SET(BUILD_NAME "${BUILD_NAME}-TV-${PORT_VERSION}")
ELSEIF(SOFTTV)
	target_compile_definitions(${PROJECT_NAME} PRIVATE SOFTTV)
	target_link_libraries(${PROJECT_NAME} PRIVATE tv-software)
	SET(BUILD_NAME "${BUILD_NAME}-TV-SOFT-${PORT_VERSION}")
ELSEIF(HDMI_DVI)
    # Phase 3b: Mac 1bpp HDMI/DVI via libdvi. 800x600@60 -> TMDS bit = sysclk =
    # 400 MHz (the Phase 2 clock contract already forces this at runtime).
    IF(PICO_BOARD STREQUAL "murmulator2")
        SET(DVI_PIN_CFG murmulator2_cfg)
    ELSE()
        SET(DVI_PIN_CFG murmulator_cfg)
    ENDIF()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        HDMI_DVI DVI_MONOCHROME_TMDS=1 DVI_DEFAULT_SERIAL_CONFIG=${DVI_PIN_CFG})
    target_link_libraries(${PROJECT_NAME} PRIVATE libdvi hdmi-dvi)
    SET(CPU_MHZ 400)
    string(REGEX REPLACE "-[0-9]+$" "-${CPU_MHZ}" BUILD_NAME "${BUILD_NAME}")  # fix stale MHz in name
    SET(BUILD_NAME "${BUILD_NAME}-HDMI-DVI-${PORT_VERSION}")
ELSE()
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGA_DRV)
    target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
    SET(BUILD_NAME "${BUILD_NAME}-VGA-${PORT_VERSION}")
ENDIF()

IF(BENCH_EMU)
    SET(BUILD_NAME "${BUILD_NAME}-BENCH")
ENDIF()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

target_link_libraries(${PROJECT_NAME} PRIVATE
        psram
        audio
        graphics
        picomp3lib

        nespad
        sdcard
        fatfs

        pico_runtime
        pico_stdlib
        pico_multicore
        hardware_dma
        hardware_pio
        hardware_i2c
        hardware_interp
        hardware_timer
        hardware_clocks
        hardware_pwm
        hardware_flash

        tinyusb_host tinyusb_board
)

IF(KBDUSB)
    add_subdirectory(drivers/ps2kbd)
    target_link_libraries(${PROJECT_NAME} PRIVATE ps2kbd)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KBDUSB)
ELSE()
    add_subdirectory(drivers/ps2)
    target_link_libraries(${PROJECT_NAME} PRIVATE ps2)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KBDPS2)
ENDIF()

IF(MURM2)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PORT_VERSION="${PORT_VERSION}"
        CPU_MHZ=${CPU_MHZ}

        # --- pins provided by boards/${PICO_BOARD}.h (Phase 2b) ---
        
        PICO_FLASH_SIZE_BYTES=${FLASH_SIZE}*1024
        DEFAULT_THROTTLING=0
    )
ELSE()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PORT_VERSION="${PORT_VERSION}"
        CPU_MHZ=${CPU_MHZ}

        # --- pins provided by boards/${PICO_BOARD}.h (Phase 2b) ---
        
        PICO_FLASH_SIZE_BYTES=${FLASH_SIZE}*1024
        DEFAULT_THROTTLING=0

        DISC_NUM_DRIVES=2
 #       DISP_WIDTH=512
#        DISP_HEIGHT=342
#        UMAC_MEMSIZE=208
        UMAC_MEMSIZE=${UMAC_MEMSIZE_VAL}
        # Debug on the Mac's native 512x342 (uses umac-rom-*-orig.h). The 640x480
        # mode is not fully supported by Mac software; native res avoids confusing
        # emulator bugs with incomplete non-standard-resolution support.
        USE_VGA_RES=0
        DISP_WIDTH=512
        DISP_HEIGHT=342
        #DISP_WIDTH=320
        #DISP_HEIGHT=240
       MUSASHI_CNF="../external/umac/include/m68kconf.h"
    )
ENDIF()

IF(BENCH_EMU)
    # On-screen benchmark HUD (drawn into the Mac framebuffer); no UART/stdio.
    target_compile_definitions(${PROJECT_NAME} PRIVATE BENCH_EMU)
ENDIF()

# Phase 1: propagate selectable core voltage to the firmware.
target_compile_definitions(${PROJECT_NAME} PRIVATE VREG_VOLTAGE_SEL=${VREG_VOLTAGE})

#pico_set_linker_script(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/memmap.ld")
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections)
pico_add_extra_outputs(${PROJECT_NAME})
