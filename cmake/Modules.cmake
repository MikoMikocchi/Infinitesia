# Build helpers for the project's own targets.

# Warnings apply to project targets only (never to fetched dependencies).
function(infinitesia_enable_warnings target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
    if (INFINITESIA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if (INFINITESIA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Declares one module (a layer or domain of the project) as a static library built from
# every .cpp/.h under the given src/ directories (nested ones such as domains/natural/geology
# are fine). Dependencies between modules are declared explicitly here;
# tools/check_dependencies.py enforces the same layering on the #include graph, so a module
# cannot quietly reach into one that is not below it. Dependencies are transitive, so a
# consumer names only the modules it uses directly.
#   infinitesia_add_module(<target> DIRS <src subdirs>... [PUBLIC <libs>...] [PRIVATE <libs>...])
function(infinitesia_add_module target)
  cmake_parse_arguments(MODULE "" "" "DIRS;PUBLIC;PRIVATE" ${ARGN})
  # Every module directory is left out of the executable's own sources (see CMakeLists.txt).
  set_property(GLOBAL APPEND PROPERTY INFINITESIA_MODULE_DIRS ${MODULE_DIRS})
  set(module_globs "")
  foreach(dir IN LISTS MODULE_DIRS)
    list(APPEND module_globs
      "${PROJECT_SOURCE_DIR}/src/${dir}/*.cpp"
      "${PROJECT_SOURCE_DIR}/src/${dir}/*.h")
  endforeach()
  file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS ${module_globs})
  add_library(${target} STATIC ${module_sources})
  target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/src)
  if (MODULE_PUBLIC)
    target_link_libraries(${target} PUBLIC ${MODULE_PUBLIC})
  endif()
  if (MODULE_PRIVATE)
    target_link_libraries(${target} PRIVATE ${MODULE_PRIVATE})
  endif()
  infinitesia_enable_warnings(${target})
endfunction()

# Command-line tool or test executable with the project's warnings.
#   infinitesia_add_executable(<target> SOURCES <files>... [LINK <libs>...]
#                              [ASSET_DIR] [GAMEPLAY_ASSET_DIR])
# ASSET_DIR / GAMEPLAY_ASSET_DIR define INFINITESIA_ASSET_DIR / INFINITESIA_GAMEPLAY_ASSET_DIR
# as the source tree's catalog directories (programs run from a build directory read them there).
function(infinitesia_add_executable target)
  cmake_parse_arguments(EXE "ASSET_DIR;GAMEPLAY_ASSET_DIR" "" "SOURCES;LINK" ${ARGN})
  add_executable(${target} ${EXE_SOURCES})
  target_link_libraries(${target} PRIVATE ${EXE_LINK})
  if (EXE_ASSET_DIR)
    target_compile_definitions(${target} PRIVATE
      INFINITESIA_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
  endif()
  if (EXE_GAMEPLAY_ASSET_DIR)
    target_compile_definitions(${target} PRIVATE
      INFINITESIA_GAMEPLAY_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets/gameplay")
  endif()
  infinitesia_enable_warnings(${target})
endfunction()

# A doctest executable whose TEST_CASEs are each registered as a CTest test. Sources that
# do not bring their own main get tests/support/DoctestMain.cpp via MAIN.
#   infinitesia_add_test(<target> SOURCES <files>... [LINK <libs>...] [MAIN]
#                        [ASSET_DIR] [GAMEPLAY_ASSET_DIR])
function(infinitesia_add_test target)
  cmake_parse_arguments(TEST "MAIN;ASSET_DIR;GAMEPLAY_ASSET_DIR" "" "SOURCES;LINK" ${ARGN})
  if (TEST_MAIN)
    list(PREPEND TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/support/DoctestMain.cpp")
  endif()
  set(forwarded "")
  if (TEST_ASSET_DIR)
    list(APPEND forwarded ASSET_DIR)
  endif()
  if (TEST_GAMEPLAY_ASSET_DIR)
    list(APPEND forwarded GAMEPLAY_ASSET_DIR)
  endif()
  infinitesia_add_executable(${target}
    SOURCES ${TEST_SOURCES}
    LINK ${TEST_LINK} TestSupport
    ${forwarded})
  doctest_discover_tests(${target})
endfunction()
