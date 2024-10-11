add_executable(mygsql driver.cpp)
target_include_directories(mygsql PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(mygsql base storage engine sql-lang)