#
# Copyright 2016-2017 Iaroslav Zeigerman
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -DLOG_USE_COLOR -std=gnu99 -O2 -fPIC")

file(GLOB SOURCE_FILES "*.c")

add_library(cluster_obj OBJECT ${SOURCE_FILES})
add_library(cluster SHARED $<TARGET_OBJECTS:cluster_obj>)
add_library(cluster_static STATIC $<TARGET_OBJECTS:cluster_obj>)
message("Current source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message("Full path to kx_config.h: ${CMAKE_CURRENT_SOURCE_DIR}/kx_config.h")
target_include_directories(cluster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(INSTALL_INCLUDE_FILES kx_gossip.h kx_network.h kx_config.h kx_errors.h)
install(FILES ${INSTALL_INCLUDE_FILES} DESTINATION include/cluster)
install (TARGETS cluster cluster_static
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib)