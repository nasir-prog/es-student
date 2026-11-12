add_library(stdio_text_protocol STATIC src/stdio-text-protocol.c)
target_include_directories(stdio_text_protocol PUBLIC include)
target_link_libraries(stdio_text_protocol PRIVATE pico_stdlib)
target_compile_options(stdio_text_protocol PRIVATE -Wall -Wextra)
