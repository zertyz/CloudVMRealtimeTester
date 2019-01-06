# Build this tool with:
#	TYPE="Release"; rm -fr $TYPE; mkdir $TYPE; cd $TYPE; CXX=/usr/bin/clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=$TYPE ..; cmake --build .; cd ..; ls -l "$TYPE/CloudVMRealtimeTester"
# and run it with:
#	Release/CloudVMRealtimeTester
# Please, make sure the dependent libraries (listed bellow) were built with Release target.

cmake_minimum_required (VERSION 3.9)
project                (CloudVMRealtimeTester VERSION 2019.01.05)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} -march=native -mcpu=native -mtune=native")	# binary is optimized, but machine dependent


# CloudVMRealtimeTester executable
##################################
file(GLOB_RECURSE SOURCE_FILES  cpp/*.h cpp/*.cpp)
#set(SOURCE_FILES cpp/ConstexprRandomUniqueStdStringKeys.cpp cpp/ConstexprRandomUniqueEastlStringKeys.cpp cpp/HashTableSpikes.cpp)
message("Building main executable './${CMAKE_BUILD_TYPE}/${PROJECT_NAME}' with: ${SOURCE_FILES}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})


# imported mutua libraries
##########################
# mutua libraries have the inclues inside the "cpp/" directory

foreach (_referencedLib CppUtils)
	set(_referencedLib_SOURCES "../../${_referencedLib}/cpp/")
	message("    --> searching library '${_referencedLib}' in '~/.cmake/packages/${_referencedLib}' with headers in '${_referencedLib_SOURCES}'")
	if (NOT TARGET ${_referencedLib})
		find_package(${_referencedLib} CONFIG REQUIRED)
		add_library (${_referencedLib} STATIC IMPORTED GLOBAL)
		message("        (built in '${${_referencedLib}_DIR}')")
	else()
		add_library(mutua::${_referencedLib} ALIAS ${_referencedLib})
		message("        (defined by the parent CMakeLists.txt)")
	endif()
	include_directories("${_referencedLib_SOURCES}")
	target_link_libraries(${PROJECT_NAME}       PRIVATE mutua::${_referencedLib})
endforeach()