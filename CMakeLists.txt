# CMakeLists.txt has to be located in the project folder and cmake has to be
# executed from 'project/build' with 'cmake ../'.
cmake_minimum_required(VERSION 3.5.1)
project(iodrivers_base
        VERSION 0.99)
find_package(Rock)
rock_activate_cxx11()
rock_init()
rock_standard_layout()
