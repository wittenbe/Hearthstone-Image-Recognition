PROJECT(hs_image_recognition)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6.4)
CMAKE_POLICY(VERSION 2.6)

SET(CMAKE_CXX_FLAGS "-std=c++11")
IF(WIN32)
	add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
ENDIF(WIN32)

add_definitions(-U__STRICT_ANSI__)
add_definitions(-DHSIR_BASE_DIR="${PROJECT_SOURCE_DIR}")

include_directories(bot)

set(EXECUTABLE_NAME ${PROJECT_NAME}_main)
add_executable(${EXECUTABLE_NAME}
    main.cpp
    Stream.cpp
    Recognizer.cpp
	PerceptualHash.cpp
	StreamManager.cpp
    bot/bot.cpp
    bot/connection.cpp
    bot/logger.cpp
    bot/mainbot.cpp
)

find_package(Boost REQUIRED COMPONENTS system thread regex chrono)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${Boost_LIBRARIES})

find_package(OpenCV REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})

IF(WIN32)
    TARGET_LINK_LIBRARIES(${EXECUTABLE_NAME} wsock32 ws2_32)
ENDIF(WIN32)
