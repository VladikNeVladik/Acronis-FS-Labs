#===================
# Compilation flags   
#===================

# Liburing:
LIBURING_LIB_DIR = $(abspath liburing/src/)
LIBURING_INCLUDE = $(abspath liburing/src/include)

LIBURING_SO = ${LIBURING_LIB_DIR}/liburing.so.2.1.0

LINK_TO_LIBURING = -L${LIBURING_LIB_DIR} -Wl,-rpath=${LIBURING_LIB_DIR} -luring

# My sources:
CC = gcc

CCFLAGS = -Werror -Wall -std=c99 -I $(abspath include) -I ${LIBURING_INCLUDE}

VPATH = src

HEADERS = io_uring.h logging.h
SOURCES = cp.c logging.c

OBJECTS = ${SOURCES:%.c=build/%.o}

EXECUTABLE = cp

#=================
# Compile sources  
#=================

all : ${LIBURING_SO} ${EXECUTABLE} ${SOURCES}

%.o : %.c ${HEADERS}
	${CC} -c $< ${CCFLAGS} -o $@

${EXECUTABLE}: ${OBJECTS} ${LIBURING_SO}
	${CC} ${CCFLAGS} ${OBJECTS} -o $@ ${LINK_TO_LIBURING}

DEPENDENCIES := ${OBJECTS:%.o=%.d}
-include ${DEPENDENCIES}

build/%.o : %.c
	@echo cc $<
	@mkdir -p build
	${CC} ${CCFLAGS} -MMD -MT $@ -MF build/$*.d -o $@ -c $<

#==============
# Get liburing  
#==============

${LIBURING_SO}:
	-git clone https://github.com/axboe/liburing.git
	cd liburing && ./configure
	cd liburing && $(MAKE)

#===============
# Miscellaneous  
#===============

clean:
	@rm -rf liburing
	@rm -rf ${EXECUTABLE}
	@rm -rf *.o
	@rm -rf *.d
	@rm -rf build

.PHONY: all clean
