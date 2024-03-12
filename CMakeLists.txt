############ Project Config ############
cmake_minimum_required(VERSION 3.11)
project(audionet C)


############ Library Dependencies ############
find_package(PkgConfig REQUIRED)
pkg_search_module(fftwf fftw3f IMPORTED_TARGET)


############ Targets ############
### Audio Socket Lib ###
add_library(AudioSocket STATIC
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
IF (DEFINED BASIC_LOGS)
    target_compile_definitions(AudioSocket PRIVATE LOGGING)
ENDIF()
IF (DEFINED DEBUG_LOGS)
    target_compile_definitions(AudioSocket PRIVATE LOGGING)
    target_compile_definitions(AudioSocket PRIVATE DEBUG)
ENDIF()
IF (DEFINED VERBOSE_LOGS)
    target_compile_definitions(AudioSocket PRIVATE LOGGING)
    target_compile_definitions(AudioSocket PRIVATE VERBOSE)
    target_compile_definitions(AudioSocket PRIVATE DEBUG)
ENDIF()
target_include_directories(AudioSocket PRIVATE contrib src)
target_link_libraries(AudioSocket PRIVATE pthread dl m)

target_include_directories(AudioSocket PRIVATE PkgConfig::fftwf)
target_link_libraries(AudioSocket PRIVATE PkgConfig::fftwf)

### Client ###
add_executable(AudioClient src/client.c)
target_link_libraries(AudioClient AudioSocket)

### Server ###
add_executable(AudioServer src/server.c)
target_link_libraries(AudioServer AudioSocket)

