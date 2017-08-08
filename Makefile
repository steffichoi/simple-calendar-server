PORT=51969
CFLAGS = -DPORT=\$(PORT) -g -Wall

calserver: calserver.o lists.o
	gcc $(CFLAGS) -o calserver calserver.o lists.o

calendar: calendar.o lists.o 
	gcc $(CFLAGS) -o calendar calendar.o lists.o

calserver.o: calserver.c lists.h
	gcc $(CFLAGS) -c calserver.c

calendar.o: calendar.c lists.h
	gcc $(CFLAGS) -c calendar.c

lists.o: lists.c lists.h
	gcc $(CFLAGS) -c lists.c

clean: 
	rm calendar calserver *.o