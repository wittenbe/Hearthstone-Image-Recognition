PROJECT(hs_image_recognition)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)
CMAKE_POLICY(VERSION 2.6)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
IF(WIN32)
	add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
ENDIF(WIN32)

add_definitions(-U__STRICT_ANSI__)

include_directories(bot)
include_directories(Calibration)

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
    Calibration/Calibrator.cpp
    Calibration/Calibration.cpp
)

set(CALIBRATOR_NAME ${PROJECT_NAME}_calib)
add_executable(${CALIBRATOR_NAME}
	Calibration/main.cpp
    Calibration/Calibrator.cpp
    Calibration/Calibration.cpp
    Recognizer.cpp
    PerceptualHash.cpp
)

set(TESTS_NAME ${PROJECT_NAME}_tests)
add_executable(${TESTS_NAME}
	tests/testRecognizer.cpp
    Recognizer.cpp
	PerceptualHash.cpp
)

#set(Boost_NO_SYSTEM_PATHS ON)
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.54 REQUIRED COMPONENTS system thread regex chrono)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${Boost_LIBRARIES})
target_link_libraries(${CALIBRATOR_NAME} ${Boost_LIBRARIES})
target_link_libraries(${TESTS_NAME} ${Boost_LIBRARIES})

find_package(OpenCV 2.4.6 REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
target_link_libraries(${CALIBRATOR_NAME} ${OpenCV_LIBS})
target_link_libraries(${TESTS_NAME} ${OpenCV_LIBS})

IF(WIN32)
    target_link_libraries(${EXECUTABLE_NAME} wsock32 ws2_32)
ENDIF(WIN32)
