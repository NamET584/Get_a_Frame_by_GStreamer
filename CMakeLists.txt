cmake_minimum_required(VERSION 3.10)
project(GStreamerC++)

# Tìm GStreamer thông qua pkg-config
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_AUDIO REQUIRED gstreamer-video-1.0)

# Thêm tệp thực thi
add_executable(GStreamerC++ test.cpp)

target_include_directories(GStreamerC++ PRIVATE ${GST_INCLUDE_DIRS} ${GST_AUDIO_INCLUDE_DIRS})
target_link_directories(GStreamerC++ PRIVATE ${GST_LIBRARY_DIRS} ${GST_AUDIO_LIBRARY_DIRS})
target_link_libraries(GStreamerC++ PRIVATE ${GST_LIBRARIES} ${GST_AUDIO_LIBRARIES})
target_link_libraries(GStreamerC++ PRIVATE ${OpenCV_LIBS})
