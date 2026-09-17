add_library(DebugPin STATIC
        src/DebugPin.cpp
        )

target_include_directories(DebugPin PUBLIC
        include
)

target_link_libraries(DebugPin PUBLIC
        pico_stdlib
)
