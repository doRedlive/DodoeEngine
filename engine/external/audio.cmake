add_library(audio STATIC miniaudio/miniaudio.h miniaudio/miniaudio_impl.cpp)

target_include_directories(audio PUBLIC miniaudio)

if(WIN32)
    target_link_libraries(audio PUBLIC winmm ole32)
endif()
