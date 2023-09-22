add_library(common_pico INTERFACE)

add_subdirectory(drivers/ps2)

target_sources(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/i_glob.c
        ${CMAKE_CURRENT_LIST_DIR}/i_input.c
        ${CMAKE_CURRENT_LIST_DIR}/i_system.c
        ${CMAKE_CURRENT_LIST_DIR}/piconet.c
        ${CMAKE_CURRENT_LIST_DIR}/i_timer.c
        ${CMAKE_CURRENT_LIST_DIR}/i_video.c
        ${CMAKE_CURRENT_LIST_DIR}/stubs.c

        ${CMAKE_CURRENT_LIST_DIR}/i_picosound.c
)
if (PICO_ON_DEVICE)
    target_sources(common_pico INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/blit.S
            ${CMAKE_CURRENT_LIST_DIR}/picoflash.c
            )
    pico_wrap_function(common_pico malloc)
    pico_wrap_function(common_pico calloc)
    pico_wrap_function(common_pico free)
    target_compile_definitions(common_pico INTERFACE
            PICO_HEAP_SIZE=0
            USE_ZONE_FOR_MALLOC=1

            PICO_SCANVIDEO_ENABLE_CLOCK_PIN=0
            PICO_SCANVIDEO_ENABLE_DEN_PIN=0
            PICO_SCANVIDEO_SYNC_PIN_BASE=12
            PICO_SCANVIDEO_HSYNC_PIN=12
            PICO_SCANVIDEO_VSYNC_PIN=13
            PICO_SCANVIDEO_ALPHA_PIN=-1
            PICO_SCANVIDEO_COLOR_PIN_BASE=6
            PICO_SCANVIDEO_COLOR_PIN_COUNT=6
            PICO_SCANVIDEO_PIXEL_RSHIFT=0
            PICO_SCANVIDEO_PIXEL_GSHIFT=2
            PICO_SCANVIDEO_PIXEL_BSHIFT=4
            PICO_SCANVIDEO_PIXEL_RCOUNT=2
            PICO_SCANVIDEO_PIXEL_GCOUNT=2
            PICO_SCANVIDEO_PIXEL_BCOUNT=2
    )
    target_link_libraries(common_pico INTERFACE  hardware_i2c ps2)
endif()
target_include_directories(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../textscreen
        )

target_compile_definitions(common_pico INTERFACE
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_DMA_IRQ=1
        NO_USE_UART=1
        NO_USE_NET=1
        #PICO_DEFAULT_UART_TX_PIN=0
        #PICO_DEFAULT_UART_RX_PIN=1
        )

pico_generate_pio_header(common_pico ${CMAKE_CURRENT_LIST_DIR}/video_doom.pio)
target_link_libraries(common_pico INTERFACE pico_stdlib pico_multicore pico_scanvideo_dpi)

add_library(pico_cd INTERFACE)
if (TARGET tinyusb_host)
    target_link_libraries(pico_cd INTERFACE tinyusb_host)
endif()