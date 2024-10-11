add_library(storage STATIC
    "storage-manager.cpp"
    "storage-table.cpp"
    "storage-database.cpp"
)
target_include_directories(storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(storage base)
