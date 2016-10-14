## Installation

# Prerequisites

-- Linux (Debian Jessie x86-64 tested).  Other CPU architectures and other
   Unices have not been tested but should work, provided that they also have:
-- A C++11-capable compiler (clang++ >= 3.3 or g++ >= 4.8.1)
-- Needed libraries: ncurses (for the ncurses-based UI), pthread
-- cmake >= 3.0.2 (for configuration)

# Building

git clone --depth 1 git://github.com/LucianLandry/arctic.git
mkdir arctic-build
cd arctic-build
cmake ../arctic
make

The ncurses-based UI is built for an 80x25 console (not 80x24, as is common).  If your window is too small, you will likely experience visual artifacts.
