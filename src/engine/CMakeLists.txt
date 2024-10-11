add_library(engine STATIC
    "engine-table.cpp"
    "engine-database.cpp"
    "engine-database-manager.cpp"
    "engine.cpp"
)
target_include_directories(engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(engine storage)