# Audionet

## Linux dependencies installation
    sudo apt install pkg-config fftw3 fftw3-dev cmake make

## Windows dependencies installation
With Administrator `powershell` install `chocolaty` windows package manager:

    Set-ExecutionPolicy AllSigned
    Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

Verify the installation:

    C:\WINDOWS\system32>choco
    Chocolatey v0.10.15
    Please run 'choco -?' or 'choco  -?' for help menu.

Install `pkg-config`:

    choco install pkgconfiglite

## Build
    cmake -S . -B build
    cmake --build build

### Linux output
The output binary will be at `build/AudioLink`

### Windows output
The output binary will be at `TODO`

## Run
    TODO

## Useful links
Web based [SoundAnalyzer](https://www.compadre.org/osp/pwa/soundanalyzer/)