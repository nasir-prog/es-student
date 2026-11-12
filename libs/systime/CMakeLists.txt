add_library(systime STATIC src/systime.c)
target_include_directories(systime PUBLIC include)
target_link_libraries(systime PRIVATE pico_stdlib)
target_compile_options(systime PRIVATE -Wall -Wextra)
