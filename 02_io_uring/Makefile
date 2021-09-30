CCFLAGS += -Werror -Wall -std=c99

all : cp

% : %.c IO_Ring.h Logging.h
	gcc -I vendor $< ${CCFLAGS} -o $@

clean:
	rm ./cp

.PHONY: all clean
