aux_source_directory(. SRC_LIST)

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)

add_executable(ChatClient ${SRC_LIST})
target_link_libraries(ChatClient muduo_net muduo_base pthread)