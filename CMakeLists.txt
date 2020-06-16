cmake_minimum_required(VERSION 2.8)

project(rdmacc C CXX)
enable_language(CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)


# Compiler Options
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

#add_compile_options(-fopt-info-vec-all)
add_compile_options(-Wall -Wextra -pthread)


option(USE_VTUNE "Control VTune from within the application" OFF)

set(USE_VTUNE OFF)

# Search for VTune
if(USE_VTUNE)
    #set(INTELAMPLIFIER_INSTALL_DIR "/opt/intel/vtune_amplifier/")
    set(INTELAMPLIFIER_INSTALL_DIR "/home/dm-lroot/intel/vtune_profiler")
    find_package(IntelAmplifier)

    if(INTELAMPLIFIER_FOUND)
        message("VTune libraries found")
        add_definitions(-DUSE_VTUNE)
    else()
        message(WARNING "VTune libraries not found")
    endif()
endif()


# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -std=c++14 -Wall -Wextra -Werror -pthread -O3 -g" )
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -std=c++14 -Wall -Wextra -Werror -pthread -DDEBUG -fsanitize=address -g -O0 -fno-inline-functions " )
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -std=c++14 -Wall -Wextra -Werror -pthread -DDEBUG  -g -O0 -fno-inline-functions -fno-omit-frame-pointer" )
# the compiler flags for compiling C++ sources
MESSAGE( STATUS "CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS} )

add_subdirectory(src)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/src/conf/RDMA.conf
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/conf/
)


