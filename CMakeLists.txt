cmake_minimum_required(VERSION 3.0.0)
cmake_policy(SET CMP0069 NEW)

project(brainc VERSION 0.1.0)
include(CTest)
include(CheckIPOSupported)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
find_package(LLVM 12 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

check_ipo_supported(RESULT supported OUTPUT error)

add_executable(brainc main.cpp ast.cpp)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET brainc PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

llvm_map_components_to_libnames(llvm_libs support core irreader)

foreach(target ${LLVM_TARGETS_TO_BUILD})
  list(APPEND targets "LLVM${target}CodeGen")
endforeach()

# Link against LLVM libraries

target_link_libraries(brainc ${llvm_libs} ${targets})

target_compile_options(brainc PUBLIC -fstandalone-debug)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)