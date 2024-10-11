cmake_minimum_required(VERSION 3.5)

project('mygsql')
set(CMAKE_CXX_STANDARD 23)

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message("CCache Found")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# # 检查编译器是否支持ASan
# if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
#     # 添加AddressSanitizer编译选项
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
#     # 对于链接器，也可能需要添加相应的选项
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
# endif()

add_subdirectory(base)
add_subdirectory(storage)
add_subdirectory(engine)
add_subdirectory(sql-lang)
add_subdirectory(driver)

# add_executable(mygsql driver/driver.cpp)