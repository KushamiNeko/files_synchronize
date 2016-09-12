ifeq ($(BUILD),debug)   
FLAGS = -Og -Wall -g -DDEBUG 
else
FLAGS = -O3 -Wall -Werror -s  
endif

GENERAL_HELPER = ~/programming_projects/c/general/bin

bin/main :  
	gcc $(FLAGS) -fopenmp `pkg-config --cflags glib-2.0 gio-2.0` \
-o bin/main \
$(GENERAL_HELPER)/general_helper \
src/main.c -lm `pkg-config --libs glib-2.0 gio-2.0`

clean:
	@if [ $(shell find 'bin' -type d -empty)  ]; then\
		echo 'bin is already clean';\
	else\
		echo 'cleaning bin ...';\
		rm -r bin/*;\
	fi

release:
	make clean
	make

debug:
	make clean
	make "BUILD=debug"

test:
	make debug

