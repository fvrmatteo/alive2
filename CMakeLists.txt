cmake_minimum_required(VERSION 3.8)

project(Alive2)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (MSVC)
  set(CMAKE_CXX_FLAGS                "/GL /EHsc /W2 ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /Oi /Oy /Zc:inline ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Oi /Zi ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_EXE_LINKER_FLAGS         "/LTCG:INCREMENTAL ${CMAKE_EXE_LINKER_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS                "-Wall -march=native -fPIC ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "-Werror -g -Og ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (CYGWIN)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
endif()

find_package(Z3 4.8.4 REQUIRED)
include_directories(${Z3_INCLUDE_DIR})

find_program(RE2C re2c)
message(STATUS "RE2C: ${RE2C}")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tools"
                   COMMAND ${RE2C} ARGS "-d" "-b" "-T" "--no-generation-date"
                   "-o" "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   "${PROJECT_SOURCE_DIR}/tools/alive_lexer.re"
                   DEPENDS "tools/alive_lexer.re")

include_directories(${PROJECT_SOURCE_DIR})

set(IR_SRCS
  ir/constant.cpp
  ir/function.cpp
  ir/instr.cpp
  ir/memory.cpp
  ir/state.cpp
  ir/state_value.cpp
  ir/type.cpp
  ir/value.cpp
)

add_library(ir STATIC ${IR_SRCS})

set(SMT_SRCS
  smt/ctx.cpp
  smt/expr.cpp
  smt/smt.cpp
  smt/solver.cpp
)

add_library(smt STATIC ${SMT_SRCS})

set(TOOLS_SRCS
  tools/transform.cpp
)

add_library(tools STATIC ${TOOLS_SRCS})

set(UTIL_SRCS
  util/compiler.cpp
  util/config.cpp
  util/errors.cpp
  util/file.cpp
  util/symexec.cpp
)

add_library(util STATIC ${UTIL_SRCS})

set(ALIVE_LIBS ir smt tools util)


if (BUILD_LLVM_UTILS OR BUILD_TV)
  find_package(LLVM REQUIRED CONFIG)

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  if (NOT LLVM_ENABLE_RTTI)
    message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_RTTI=ON'")
  endif()

  if (NOT LLVM_ENABLE_EH)
    message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_EH=ON'")
  endif()

  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)

  include_directories(${LLVM_INCLUDE_DIRS})
  add_definitions(${LLVM_DEFINITIONS})

  set(LLVM_UTIL_SRCS
    "llvm_util/utils.cpp"
  )

  add_library(llvm_util STATIC ${LLVM_UTIL_SRCS})
  set(ALIVE_LIBS_LLVM llvm_util ${ALIVE_LIBS})

  add_llvm_executable(alive-tv
    "tools/alive-tv.cpp"
  )

else()
  set(LLVM_UTIL_SRCS "")
endif()

if (BUILD_TV)
  if (CYGWIN)
    message(FATAL_ERROR "LLVM plugins not supported on cygwin")
  endif()
  message(STATUS "Compiling translation validation plugin")
  add_subdirectory(tv)
else()
  message(STATUS "Skiping translation validation plugin")
endif()

add_executable(alive
               "tools/alive.cpp"
               "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
               "tools/alive_parser.cpp"
              )
target_link_libraries(alive PRIVATE ${ALIVE_LIBS} pthread)

add_library(alive2 SHARED ${IR_SRCS} ${SMT_SRCS} ${TOOLS_SRCS} ${UTIL_SRCS} ${LLVM_UTIL_SRCS})

if (BUILD_LLVM_UTILS OR BUILD_TV)
  llvm_map_components_to_libnames(llvm_libs support core irreader)
  target_link_libraries(alive2 PRIVATE ${llvm_libs})
  target_link_libraries(alive-tv PRIVATE ${ALIVE_LIBS_LLVM} ${llvm_libs})
endif()

if (CYGWIN)
  # cmake on cygwin doesn't seem to know about dlls..
  get_filename_component(Z3_LIB_DIR "${Z3_LIBRARIES}" DIRECTORY)
  set(CMAKE_EXE_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  set(CMAKE_SHARED_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  file(COPY "${Z3_LIBRARIES}" DESTINATION "${PROJECT_BINARY_DIR}")
else()
  target_link_libraries(alive PRIVATE ${Z3_LIBRARIES} pthread)
  target_link_libraries(alive2 PRIVATE ${Z3_LIBRARIES} pthread)
endif()

add_custom_target("check"
                  COMMAND "python"
                          "${PROJECT_SOURCE_DIR}/tests/lit/lit.py"
                          "-s"
                          "${PROJECT_SOURCE_DIR}/tests"
                  DEPENDS "alive"
                  USES_TERMINAL
                 )
