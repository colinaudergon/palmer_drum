add_library(ButtonInput STATIC
        src/ButtonInput.cpp
        )

target_include_directories(ButtonInput PUBLIC
        include
)

target_link_libraries(ButtonInput PUBLIC
        pico_stdlib
        DigitalInput
)
