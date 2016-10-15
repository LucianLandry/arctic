## Introduction

This is the source code for "arctic", a simple [chess engine](https://en.wikipedia.org/wiki/Chess_engine).

## Supported Platforms

Linux 64-bit (tested) and 32-bit should work.  Should be compatible with most Unices that support a C++11-capable compiler.  See INSTALL.md for more details.

## Features

The engine supports a basic multi-threaded minimax search, transposition table, history heuristic, and futility pruning.  The evaluation is extremely simplistic, almost completely material-based except for some endgame scenarios.  Be prepared for some "interesting" moves.

Many chess engines require a separate GUI to run.  To this end, CECP (xboard) and UCI interfaces are provided.  A simplistic ncurses-based interface is also currently provided.

## License

See the LICENSE file for details.  The current main license is [MPL 2.0](https://mozilla.org/MPL/2.0/).

## Motivation

Building anything that seems to "think" is fun.  I am more interested in generalized search than obtaining the highest possible strength.  I distribute it in the hope that someone may find it interesting/useful, as a work sample, and for the open-source warm fuzzies.

If you are interested in a C version of the code, the git history may be helpful since arctic was originally written in C.
