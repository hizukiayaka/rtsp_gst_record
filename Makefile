
libs=`pkg-config --cflags --libs gstreamer-1.0` `pkg-config --cflags --libs glib-2.0`

server:
	gcc -Wall -DDEBUG -g server.c configparse.c -o server $(libs)

clean:
	rm -f *.o server

.PHONY=clean
