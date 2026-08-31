add_library(GateInput STATIC
        src/GateInput.cpp
        )

target_include_directories(GateInput PUBLIC
        include
)

target_link_libraries(GateInput PUBLIC
        pico_stdlib
)