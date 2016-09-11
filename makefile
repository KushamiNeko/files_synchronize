ifeq ($(BUILD),debug)   
FLAGS = -O0 -Wall -g
else
FLAGS = -O3 -Wall -Werror -s -DNDEBUG
endif

GENERAL_HELPER = ~/programming_projects/c/general/bin

#echo:
#	echo $(TEST)/aa

#bin/rectangle_test : bin/main 
#	gcc $(FLAGS) `pkg-config --cflags gtk+-3.0 glew glib-2.0 cmockery2` \
#-o bin/rectangle_test \
#$(GENERAL_HELPER)/general_helper \
#src/rectangle_test.c -lm \
#`pkg-config --libs gtk+-3.0 glew glib-2.0 cmockery2`

bin/main :  
	gcc $(FLAGS) `pkg-config --cflags glib-2.0 gio-2.0` \
-o bin/main \
$(GENERAL_HELPER)/general_helper \
src/main.c -lm `pkg-config --libs glib-2.0 gio-2.0`


#bin/pattern_control: bin/pattern_model 
#	gcc $(FLAGS) `pkg-config --cflags gtk+-3.0 glew` -c src/pattern_control.c \
#`pkg-config --libs gtk+-3.0 glew` -o bin/pattern_control 
#
#
#bin/pattern_model: bin/rectangle 
#	gcc $(FLAGS) `pkg-config --cflags gtk+-3.0 glew` -c src/pattern_model.c \
#	 	-o bin/pattern_model `pkg-config --libs gtk+-3.0 glew`
#
#bin/rectangle:
#	gcc $(FLAGS) -c src/rectangle.c -o bin/rectangle

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
	bin/rectangle_test

