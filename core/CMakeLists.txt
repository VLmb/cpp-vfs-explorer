cmake_minimum_required(VERSION 3.20)
project(vfs_core_logic)

set(CMAKE_CXX_STANDARD 17)

# Подключаем все .cpp и .h файлы, которые лежат в этой папке
file(GLOB_RECURSE SOURCES *.cpp *.h)

# Говорим CLion, что это библиотека (чтобы он проиндексировал код)
add_library(core_lib ${SOURCES})

# Если вдруг понадобятся стандартные библиотеки (filesystem и т.д.)
# target_link_libraries(core_lib)