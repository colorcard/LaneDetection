cmake_minimum_required(VERSION 3.10)
project(LaneDetection VERSION 1.0)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# 添加可执行文件
add_executable(lane_detection main.c)

# 在Windows平台上禁用安全函数警告
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # 非Windows平台才链接数学库
    target_link_libraries(lane_detection m)
endif()

# 下载STB依赖库
include(FetchContent)
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# 设置include路径
target_include_directories(lane_detection PRIVATE ${stb_SOURCE_DIR})

# 显示编译信息
message(STATUS "配置车道线检测项目完成")

# 设置安装规则
install(TARGETS lane_detection DESTINATION bin)

# 添加一个目标用于手动下载STB库文件
add_custom_target(download_stb_headers
        COMMAND ${CMAKE_COMMAND} -E make_directory stb_headers
        COMMAND ${CMAKE_COMMAND} -E chdir stb_headers curl -L -O https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
        COMMAND ${CMAKE_COMMAND} -E chdir stb_headers curl -L -O https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "下载STB头文件到stb_headers目录"
)