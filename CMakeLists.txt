# CMakeLists.txt has to be located in the project folder and cmake has to be
# executed from 'project/build' with 'cmake ../'.
cmake_minimum_required(VERSION 3.9)
project(iodrivers_base
        VERSION 0.99
        DESCRIPTION "comprehensive support to handle packetized streams on any I/O")
find_package(Rock)
rock_activate_cxx11()
rock_init()
rock_standard_layout()
