cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(VLA_ORIN VERSION 1.0.0 LANGUAGES C CXX)

# 设置C/C++标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 交叉编译工具链设置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 设置交叉编译工具链路径
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 设置交叉编译工具链查找路径
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/mos_comm)
include_directories(${CMAKE_SOURCE_DIR}/include/trro)

# 库文件目录
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
link_directories(${LIB_DIR})

# 源文件目录
set(SOURCES_DIR ${CMAKE_SOURCE_DIR}/src)

# 收集所有源文件
file(GLOB_RECURSE C_SOURCES "${SOURCES_DIR}/*.c")
file(GLOB_RECURSE CPP_SOURCES "${SOURCES_DIR}/*.cpp")

# 创建可执行文件（命名为vlaOrin）
add_executable(vlaOrin
    ${C_SOURCES}
    ${CPP_SOURCES}
)

add_compile_definitions(_GNU_SOURCE)

# ==================== 链接 cJSON 库 ====================
find_library(CJSON_LIB
    NAMES cjson
    PATHS ${LIB_DIR}
    NO_DEFAULT_PATH
)

if(CJSON_LIB)
    message(STATUS "找到 cJSON 库: ${CJSON_LIB}")
    target_link_libraries(vlaOrin PRIVATE ${CJSON_LIB})
else()
    message(FATAL_ERROR "未找到 libcjson.so，请确认已放入 ${LIB_DIR}")
endif()


# ==================== 链接 MOS 通信库 ====================
find_library(MOS_COMM_LIB
    NAMES c_communication communication
    PATHS ${LIB_DIR}
    NO_DEFAULT_PATH
)
if(MOS_COMM_LIB)
    message(STATUS "找到MOS库: ${MOS_COMM_LIB}")
    target_link_libraries(vlaOrin PRIVATE ${MOS_COMM_LIB})
else()
    message(WARNING "未找到MOS库文件，请确保库文件已放置在lib目录中")
    target_link_libraries(vlaOrin PRIVATE c_communication communication)
endif()

# ==================== 链接 腾讯 TRRo RTC 库（nvidia版） ====================
set(TRRO_LIBS
    trro_field      # 主接口库，导出所有 TRRO_* 函数
    rtc_media       # 媒体处理（外部编码流注入等）
    rtc_engine      # 核心 WebRTC 引擎
)

set(TRRO_FOUND_LIBS "")
foreach(lib_name IN LISTS TRRO_LIBS)
    find_library(${lib_name}_LIB
        NAMES ${lib_name}
        PATHS ${LIB_DIR}
        NO_DEFAULT_PATH
    )
    if(${lib_name}_LIB)
        message(STATUS "找到 TRRo 库: ${${lib_name}_LIB}")
        list(APPEND TRRO_FOUND_LIBS ${${lib_name}_LIB})
    else()
        message(WARNING "未找到 TRRo 库: lib${lib_name}.so ，请确保已放入 ${LIB_DIR} 目录")
        list(APPEND TRRO_FOUND_LIBS ${lib_name})
    endif()
endforeach()

# 按正确依赖顺序链接（trro_field -> rtc_media -> rtc_engine）
target_link_libraries(vlaOrin
    PRIVATE
    ${TRRO_FOUND_LIBS}
)

# ==================== 强制链接 FFmpeg 和 v4l2 依赖库（解决交叉编译依赖解析问题） ====================
# 严格按照 FFmpeg 标准依赖顺序链接（非常重要！）
target_link_libraries(vlaOrin
    PRIVATE
    ${LIB_DIR}/libavformat.so.60
    ${LIB_DIR}/libavcodec.so.60
    ${LIB_DIR}/libavfilter.so.9
    ${LIB_DIR}/libswscale.so.7
    ${LIB_DIR}/libswresample.so.4
    ${LIB_DIR}/libavutil.so.58
    ${LIB_DIR}/libavdevice.so.60
    ${LIB_DIR}/libv4l2.so.0
    ${LIB_DIR}/libv4lconvert.so.0
    ${LIB_DIR}/libjpeg.so.8
)

# ==================== 链接系统库 ====================
target_link_libraries(vlaOrin
    PRIVATE
    pthread
    dl
    rt
)

target_include_directories(vlaOrin
    PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

# ==================== 安装规则（可选） ====================
install(TARGETS vlaOrin
    RUNTIME DESTINATION bin
)

# ==================== rpath 设置（运行时能找到同目录下的 lib） ====================
set_target_properties(vlaOrin PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/lib"
)

# ==================== Protobuf + zlib (Orin aarch64 专用配置) ====================

# 1️⃣ 指定 aarch64 protobuf 库和 zlib 库
set(PROTOBUF_LIBRARY "${CMAKE_SOURCE_DIR}/lib/libprotobuf.so")
set(ZLIB_LIBRARY     "${CMAKE_SOURCE_DIR}/lib/libz.so")

# 2️⃣ 添加头文件到目标
# SYSTEM 表示这是系统/第三方头文件，编译器不会对这个目录下的文件显示警告
target_include_directories(vlaOrin
    SYSTEM
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include    # 包含 google/protobuf 和 can_gateway.pb.h
)

# 3️⃣ 添加 protobuf 生成的源文件
target_sources(vlaOrin
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/can_gateway.pb.cc
)

# 4️⃣ 屏蔽 protobuf 源文件的警告
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/can_gateway.pb.cc PROPERTIES COMPILE_FLAGS "-w")

# 5️⃣ 链接 protobuf 库
if(EXISTS ${PROTOBUF_LIBRARY})
    message(STATUS "使用 aarch64 protobuf 库: ${PROTOBUF_LIBRARY}")
    target_link_libraries(vlaOrin PRIVATE ${PROTOBUF_LIBRARY})
else()
    message(FATAL_ERROR "未找到 aarch64 protobuf 库: ${PROTOBUF_LIBRARY}")
endif()

# 6️⃣ zlib aarch64 库
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")  # 如果头文件也放在 include
target_include_directories(vlaOrin PRIVATE ${ZLIB_INCLUDE_DIR})
target_link_libraries(vlaOrin PRIVATE ${ZLIB_LIBRARY})
