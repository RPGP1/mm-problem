cmake_minimum_required(VERSION 3.8)


# flags for executable file
add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-Og\ -g>
    $<$<CONFIG:RelWithDebgInfo>:-O3\ -g>
    )
# flags for compile errors
add_compile_options(
    "-ftemplate-backtrace-limit=0"
    "-fmax-errors=10"
    )
# flags for warnings
set(WARNING_FLAGS
    all
    extra
    conversion
    switch-default
    disabled-optimization
    format
    init-self
    overloaded-virtual
    float-equal
    old-style-cast
    return-type
    )
foreach(FLAG IN LISTS WARNING_FLAGS)
    add_compile_options(
        -W${FLAG}
        )
endforeach()


project(MM_PROBLEM CXX)

file(GLOB_RECURSE MM_PROBLEM_SOURCES
    "src/*.cpp")
add_library(mm_problem INTERFACE)
target_sources(mm_problem INTERFACE
    "${MM_PROBLEM_SOURCES}")
target_include_directories(mm_problem INTERFACE
    "include")

target_compile_features(mm_problem INTERFACE
    cxx_std_17)
