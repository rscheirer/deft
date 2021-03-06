enable_testing()

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/tests/generated-haskell)
macro (add_simple_tests_for library)
  foreach(name ${ARGN})
    add_executable(tests/${name}.test tests/${name}.cpp)
    target_link_libraries(tests/${name}.test ${library})
    add_test(${name} tests/${name}.test)
  endforeach(name)
endmacro (add_simple_tests_for)
