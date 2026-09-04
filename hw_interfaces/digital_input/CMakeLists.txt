add_library(DigitalInput STATIC
        src/DigitalInput.cpp
        )

target_include_directories(DigitalInput PUBLIC
        include
)

target_link_libraries(DigitalInput PUBLIC
        pico_stdlib
)