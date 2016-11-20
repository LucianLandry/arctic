# Installation
## Prerequisites

- Linux (tested w/Debian Jessie x86-64).  Other CPU architectures and other
   Unices have not been tested but should work, provided that they also have:
- A C++11-capable compiler (tested w/clang.  clang++ >= 3.3 or g++ >= 4.8.1 should work.)
- C++ RTTI support.
- Needed libraries: ncurses (for the ncurses-based UI), pthread
- cmake >= 2.8.11 (for configuration)

## Building
```
$ git clone --depth 1 git://github.com/LucianLandry/arctic.git
$ mkdir arctic-build
$ cd arctic-build
$ cmake ../arctic
$ make
```
(the 'arctic' executable should now appear in your build directory, and may be copied/moved anywhere you need.)

## Running

```arctic --help``` will display usage, including how to configure the UI interface.

The ncurses-based UI is built for an 80x25 console (not 80x24, as is common).  If your window is too small, you will likely experience visual artifacts.
