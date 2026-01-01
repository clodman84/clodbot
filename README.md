# clodbot
## display

Raylib based GUI in C. Writes to a 4.3" touchscreen using Linux DRM on the Raspberry Pi, just opens a window on my development computer. Communicates to other processes on the device using a unix socket open on /tmp/display_ctrl.sock.

## messenger

A message broker that allows the services to talk to each other and handles commands from the client ( me !! ) using websockets, unix sockets, and SPI (for the microcontroller)
