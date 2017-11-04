cmake_minimum_required(VERSION 3.1)

project(use_stlabs)
include_directories(
	D:/PRogramming/boost/boost_1_64_0/boost_1_64_0
)

add_executable(test_prog
	main.cxx
	examples.h
	examples.cxx
	)