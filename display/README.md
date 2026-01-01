# Display application for clodbot

## Build Instructions

Assumes that raylib was compiled with DRM flags on the Raspberry Pi 4B.

On dev machine

`
cmake -B build
cmake --build build
`

On raspi

`
cmake -B build -DRASPI=ON
cmake --build build
`
