############ Project Config ############
cmake_minimum_required(VERSION 3.11)
project(audionet C)


############ Library Dependencies ############
find_package(PkgConfig REQUIRED)
pkg_search_module(fftwf fftw3f IMPORTED_TARGET)


############ Targets ############
### Audio Socket Lib ###
add_library(AudioLink STATIC
        src/fft/fft.c
        src/utils/utils.c
        src/audio/audio.c
        src/audio/internal/miniaudio.c
        src/audio/internal/multi_waveform_data_source.c
        src/audio_socket/audio_socket.c
        src/audio_socket/layers/link/link_layer.c
        src/audio_socket/layers/physical/physical_layer.c
        src/audio_socket/layers/physical/audio_encoding.c
        src/audio_socket/layers/transport/transport_layer.c
)
target_include_directories(AudioLink PRIVATE include src)
target_link_libraries(AudioLink PRIVATE pthread dl m)

target_include_directories(AudioLink PRIVATE PkgConfig::fftwf)
target_link_libraries(AudioLink PRIVATE PkgConfig::fftwf)

### Client ###
add_executable(AudioClient src/client.c)
target_link_libraries(AudioClient AudioLink)

### Server ###
add_executable(AudioServer src/server.c)
target_link_libraries(AudioServer AudioLink)

