OBJ =  main.o cJSON/cJSON.o
CC = gcc

iotserver: ${OBJ} main.h
	$(CC) $(CFLAGS) -std=gnu99 ${OBJ} -o $@

.c.o:
	$(CC) $(CFLAGS) -std=gnu99 -c $*.c $(LDFLAGS)

test_fullstate.o: test_fullstate.c
	$(CC) $(CFLAGS) -std=gnu99 -c test_fullstate.c $(LDFLAGS)
test_fullstate: test_fullstate.o ${OBJ}
	$(CC) $(CFLAGS) -std=gnu99 cJSON/cJSON.o test_fullstate.o -o $@

clean:
	-rm *.o iotserver
