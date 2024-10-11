add_library(sql-lang STATIC
    "sql-lang-interpreter.cpp"
)
target_include_directories(sql-lang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sql-lang engine)
