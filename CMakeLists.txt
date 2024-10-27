cmake_minimum_required(VERSION 3.16)

project(viauled VERSION 1.0.0 LANGUAGES C)

find_package(hidapi REQUIRED)

add_executable(viauled
    main.c
)

target_link_libraries(viauled PRIVATE hidapi-hidraw)
