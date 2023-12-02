cmake_minimum_required(VERSION 3.0)
project(audionet C)


add_executable(AudioLink
        src/main.c
        src/audio/audio.c
        src/audio/audio.h
        src/audio/miniaudio.c
)
target_include_directories(AudioLink PRIVATE include)
target_link_libraries(AudioLink PRIVATE pthread dl m)
