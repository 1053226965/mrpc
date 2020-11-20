# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量

project(mrpc) # 项目名称

include_directories("${PROJECT_SOURCE_DIR}/../include" 
  "${PROJECT_SOURCE_DIR}/../third_party/concurrentqueue" 
  "${PROJECT_SOURCE_DIR}/../third_party/spdlog/include" 
  "${PROJECT_SOURCE_DIR}/../third_party/fmtlib/include")

aux_source_directory(. DIR_LIB_SRCS)

add_executable (utest ${DIR_LIB_SRCS}) # 要求编译可执行文件

target_link_libraries(utest utils)
