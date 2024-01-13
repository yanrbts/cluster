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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -DLOG_USE_COLOR -std=gnu99 -O2")
include_directories(../src)
include_directories(./)
add_executable(gfs kx_gfs.c kx_linenoise.c $<TARGET_OBJECTS:cluster_obj>)
add_executable(kxcluster kx_cluster.c  $<TARGET_OBJECTS:cluster_obj>)

find_package(Threads REQUIRED)
target_link_libraries(gfs PRIVATE Threads::Threads)