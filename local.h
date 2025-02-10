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

void start_reader(char *mac, char *type);

#ifdef CUMULOCITY
int relay_data();
#endif

extern char *gLOGSTRING;

extern int gVERBOSE;
