# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/cmake-3.18.1/bin/cmake

# The command to remove a file.
RM = /usr/local/cmake-3.18.1/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/gtwang/FUSE

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/gtwang/FUSE/bin

# Include any dependencies generated for this target.
include src/CMakeFiles/test_list.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/test_list.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/test_list.dir/flags.make

src/CMakeFiles/test_list.dir/test_list.c.o: src/CMakeFiles/test_list.dir/flags.make
src/CMakeFiles/test_list.dir/test_list.c.o: ../src/test_list.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/gtwang/FUSE/bin/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/CMakeFiles/test_list.dir/test_list.c.o"
	cd /home/gtwang/FUSE/bin/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_list.dir/test_list.c.o -c /home/gtwang/FUSE/src/test_list.c

src/CMakeFiles/test_list.dir/test_list.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_list.dir/test_list.c.i"
	cd /home/gtwang/FUSE/bin/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/gtwang/FUSE/src/test_list.c > CMakeFiles/test_list.dir/test_list.c.i

src/CMakeFiles/test_list.dir/test_list.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_list.dir/test_list.c.s"
	cd /home/gtwang/FUSE/bin/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/gtwang/FUSE/src/test_list.c -o CMakeFiles/test_list.dir/test_list.c.s

# Object files for target test_list
test_list_OBJECTS = \
"CMakeFiles/test_list.dir/test_list.c.o"

# External object files for target test_list
test_list_EXTERNAL_OBJECTS =

test_list: src/CMakeFiles/test_list.dir/test_list.c.o
test_list: src/CMakeFiles/test_list.dir/build.make
test_list: src/CMakeFiles/test_list.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/gtwang/FUSE/bin/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable ../test_list"
	cd /home/gtwang/FUSE/bin/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_list.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/test_list.dir/build: test_list

.PHONY : src/CMakeFiles/test_list.dir/build

src/CMakeFiles/test_list.dir/clean:
	cd /home/gtwang/FUSE/bin/src && $(CMAKE_COMMAND) -P CMakeFiles/test_list.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/test_list.dir/clean

src/CMakeFiles/test_list.dir/depend:
	cd /home/gtwang/FUSE/bin && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/gtwang/FUSE /home/gtwang/FUSE/src /home/gtwang/FUSE/bin /home/gtwang/FUSE/bin/src /home/gtwang/FUSE/bin/src/CMakeFiles/test_list.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/test_list.dir/depend
