add_library(base STATIC
    "linux/filemapper.cpp"
    "util/mtb-id-allocator.cpp"
    "sql-value.cpp")
target_include_directories(base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
