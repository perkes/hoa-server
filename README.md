# Heroes of Argentum Server

This is the Heroes of Argentum server. The source code is a migration from the original Visual Basic 6 by using a source code translator.

## Building

You need CMake, Libevent2, and boost >= 1.49.

If you're on Ubuntu, you also need to install (if you haven't alredy) the libraries before you proceed:

    sudo apt install libevent-dev libboost-date-time-dev libboost-filesystem-dev libboost-system-dev libboost-locale-dev

After all the required libraries are installed, you compile the source:

    mkdir build && cd build
    cmake ..
    make

## Running

You need websockify to convert the ws traffic from the client into TCP traffic. If you use the default settings you can do that by running this command after installing websockify:

    sudo ./run 8000 :7666

## Libevent

Libevent for Windows can be found with Nuget: https://www.nuget.org/packages/libevent_vc120/
