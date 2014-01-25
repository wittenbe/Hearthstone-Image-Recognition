PROJECT(hs_image_recognition)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7)
CMAKE_POLICY(VERSION 2.6)

SET(CMAKE_CXX_FLAGS "-std=c++11")
IF(WIN32)
	add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
ENDIF(WIN32)

add_definitions(-U__STRICT_ANSI__)

include_directories(bot)

set(EXECUTABLE_NAME ${PROJECT_NAME}_main)
add_executable(${EXECUTABLE_NAME}
    main.cpp
    Stream.cpp
    Recognizer.cpp
	PerceptualHash.cpp
	StreamManager.cpp
	CommandProcessor.cpp
    bot/bot.cpp
    bot/connection.cpp
)

#set(Boost_NO_SYSTEM_PATHS ON)
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.54 REQUIRED COMPONENTS system thread regex chrono)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${Boost_LIBRARIES})

#set(OpenCV_SHARED OFF)
find_package(OpenCV 2.4.6 REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})

IF(WIN32)
    target_link_libraries(${EXECUTABLE_NAME} wsock32 ws2_32)
ENDIF(WIN32)
