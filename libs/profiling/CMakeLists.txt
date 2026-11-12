add_library(profiling STATIC src/profiling.c)
target_include_directories(profiling PUBLIC include)
target_link_libraries(profiling PRIVATE systime)
target_compile_options(profiling PRIVATE -Wall -Wextra)
