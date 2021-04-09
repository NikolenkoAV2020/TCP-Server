cmake_minimum_required(VERSION 3.10)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../TCP-Test)

project(
    testclient
)

set(SOURCE_EXE 
    TestClient.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCE_EXE}
)

set (Boost_NO_SYSTEM_PATHS    ON)
set (Boost_USE_MULTITHREADED  ON)
set (Boost_USE_STATIC_LIBS    ON)
set (Boost_USE_STATIC_RUNTIME OFF)
set (BOOST_ALL_DYN_LINK       OFF)

find_package (Boost REQUIRED COMPONENTS 
    thread 
    system
    chrono
    regex
)
 
if (Boost_FOUND)
    message("Boost is found.")
    message("Boost include dir : ${Boost_INCLUDE_DIRS}")
    message("Boost library dir : ${Boost_LIBRARY_DIRS}")    
    message("Boost libraries   : ${Boost_LIBRARIES}")
        
    include_directories(${Boost_INCLUDE_DIRS} )   
    target_link_libraries(${PROJECT_NAME} 
        ${Boost_LIBRARIES} 
        rt        
    )     
else()
    message("Boost is not found.")
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)   
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)   
