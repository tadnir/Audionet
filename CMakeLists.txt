############ Project Config ############
cmake_minimum_required(VERSION 3.11)
project(audionet C)


############ Library Dependencies ############
find_package(PkgConfig REQUIRED)
pkg_search_module(fftwf REQUIRED fftw3f IMPORTED_TARGET)


############ Targets ############
add_executable(AudioLink
        src/main.c
        src/audio/miniaudio.c
        src/audio/audio.c
        src/fft/fft.c
)
target_include_directories(AudioLink PRIVATE include src)
target_link_libraries(AudioLink PRIVATE pthread dl m)

target_include_directories(AudioLink PRIVATE PkgConfig::fftwf)
target_link_libraries(AudioLink PRIVATE PkgConfig::fftwf)

