add_library(pwm_audio_codec STATIC
        src/pwm_audio_codec.cpp
)

# Path is resolved relative to this file (not CMAKE_SOURCE_DIR) so this library builds
# identically regardless of which top-level CMakeLists.txt (native or platform/<board>) is
# configuring the project.
target_include_directories(pwm_audio_codec PUBLIC
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(pwm_audio_codec PUBLIC
        pico_stdlib
        hardware_dma
        hardware_pwm
        hardware_irq
        hardware_clocks
        hardware_structs
)