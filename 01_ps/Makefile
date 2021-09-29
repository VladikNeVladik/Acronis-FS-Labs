CCFLAGS += -Werror -Wall -std=c99

all : ps

% : %.c
	gcc $< ${CCFLAGS} -o $@

clean:
	rm ps

.PHONY: all clean
