# 源文件
aux_source_directory(. SRC_LIST)

# 生成可执行文件
add_executable(optimizeMain ${SRC_LIST})