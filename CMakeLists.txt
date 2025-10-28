cmake_minimum_required(VERSION 3.31)
project(example)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS
        Core
        Gui
        Widgets
        PrintSupport
        Pdf
        PdfWidgets
        REQUIRED)
#qt5_wrap_cpp(MAIN_MOC main.cpp)
add_executable(example main.cpp)
target_link_libraries(example
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::Pdf
        Qt5::PdfWidgets
)

