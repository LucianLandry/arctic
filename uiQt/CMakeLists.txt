#-----------------------------------------------------------------------------
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#-----------------------------------------------------------------------------

# libJuce shouldn't need to see anything in the normal source directory, so we
# include it separately.
find_package(Qt5Widgets REQUIRED)
include_directories("${PROJECT_SOURCE_DIR}")

add_library(uiQt uiQt.cpp)
# necessary to get normal include paths.
target_link_libraries(uiQt Qt5::Widgets)
