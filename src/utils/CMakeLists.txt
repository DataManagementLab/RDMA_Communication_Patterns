enable_language(CXX)
set(UTILS_SRC
  PerfEvent.hpp
  Concurrency.h
  ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/
add_library(utils ${UTILS_SRC})
target_include_directories(utils PUBLIC ./)
target_link_libraries(utils gflags)
set_target_properties(utils PROPERTIES LINKER_LANGUAGE CXX)
