#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.2.0)

add_executable(ustream_aux_e2e
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/az_ulib_ustream_aux_e2e.c
)

ulib_populate_test_target(ustream_aux_e2e)
