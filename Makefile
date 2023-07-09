OBJ =  main.o cJSON/cJSON.o
CC = gcc

iotserver: ${OBJ} main.h
	$(CC) $(CFLAGS) -std=gnu99 ${OBJ} -o $@

.c.o:
	$(CC) $(CFLAGS) -std=gnu99 -c $*.c $(LDFLAGS)

clean:
	-rm *.o iotserver
