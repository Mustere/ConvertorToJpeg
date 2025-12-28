cmake_minimum_required(VERSION 3.16)
project(HEICConverter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Подключаем Qt5 Widgets и Concurrent
find_package(Qt5 COMPONENTS Widgets Concurrent REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBHEIF REQUIRED libheif)

include_directories(${LIBHEIF_INCLUDE_DIRS})
link_directories(${LIBHEIF_LIBRARY_DIRS})

add_executable(HEICConverter
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    heicconverter.cpp
    heicconverter.h
    settingsdialog.h settingsdialog.cpp
)

# Линкуем QtWidgets, QtConcurrent и libheif
target_link_libraries(HEICConverter Qt5::Widgets Qt5::Concurrent ${LIBHEIF_LIBRARIES})
