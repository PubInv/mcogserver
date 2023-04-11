OBJ =  main.o cJSON/cJSON.o

iotserver: ${OBJ} main.h 
	$(CC) $(CFLAGS) ${OBJ} -o $@

.c.o:
	$(CC) $(CFLAGS) -c $*.c $(LDFLAGS)

clean:
	-rm *.o iotserver
