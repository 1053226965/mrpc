# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量

project(mrpc) # 项目名称

include_directories("${PROJECT_SOURCE_DIR}/../include" 
  "${PROJECT_SOURCE_DIR}/../third_party/concurrentqueue" 
  "${PROJECT_SOURCE_DIR}/../third_party/spdlog/include" 
  "${PROJECT_SOURCE_DIR}/../third_party/fmtlib/include")

aux_source_directory(. DIR_LIB_SRCS)
aux_source_directory(./io/ DIR_LIB_SRCS)

# 生成链接库
add_library (utils ${DIR_LIB_SRCS})

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_link_libraries(utils ws2_32)
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Windows")