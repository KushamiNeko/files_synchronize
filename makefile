ifeq ($(BUILD),debug)   
FLAGS = -Og -Wall -g -DDEBUG 
else
FLAGS = -O3 -Wall -Werror -s -DNDEBUG
endif

GENERAL_HELPER = ~/programming_projects/c/general/bin

COMPILER = gcc

PKG_CONFIG_LIBS = glib-2.0 gio-2.0

bin/main :  
	$(COMPILER) $(FLAGS) `pkg-config --cflags $(PKG_CONFIG_LIBS)` \
-o bin/main \
$(GENERAL_HELPER)/general_helper \
src/main.c `pkg-config --libs $(PKG_CONFIG_LIBS)`

clean:
	rm -f bin/*

release:
	make clean
	make

debug:
	make clean
	make "BUILD=debug"

test:
	make debug

