#ifndef MAIN_H_
#define MAIN_H_

#define CUMULOCITY

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>
#include "cJSON/cJSON.h"
#include "secrets.h"

//#define DATADIR "DATA"
//#define PARAMDIR "/home/geoff/iotserver/params/DATA"
#define PARAMDIR "."

//Server control functions

// user shall implement this function
void start_server(const char *port);
void sendp(struct sockaddr_in their_addr, char *pMessage, ...);
char *FlattenJSON(cJSON *item, char *label, int values);
void debug (char *message, ...);
char *stracat(char *dest, char *src);

#ifdef CUMULOCITY
int relay_data();
#endif

//extern char *gLOGSTRING;

extern int gVERBOSE;
extern int gLISTENFD;
#endif
