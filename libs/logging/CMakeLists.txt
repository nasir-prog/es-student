add_library(logging STATIC src/log.c)
target_include_directories(logging PUBLIC include)
target_compile_options(logging PRIVATE -Wall -Wextra)
