#-----------------------------------------------------------------------------
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#-----------------------------------------------------------------------------

find_package(Qt5 COMPONENTS Widgets Svg REQUIRED)
include_directories("${PROJECT_SOURCE_DIR}")

add_library(uiQt uiQt.cpp)
# necessary to get normal include paths:
target_link_libraries(uiQt Qt5::Widgets Qt5::Svg)
