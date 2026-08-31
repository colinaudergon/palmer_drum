add_library(Leds STATIC
        src/Leds.cpp
        )

target_include_directories(Leds PUBLIC
        include
)

target_link_libraries(Leds PUBLIC
        pico_stdlib
)