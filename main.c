#include <getopt.h>

#include "main.h"


int gVERBOSE = 0;
char *gLOGSTRING = NULL;

char *gLISTEN_PORT = "57573";

char gBUF[64 * 1024] = "";
int gLISTENFD;
struct sockaddr_in gTHEIR_ADDR;
char *gPAYLOAD = NULL;
int gPAYLOAD_SIZE = 0;
char *gMETHOD = NULL;
char *gURI = NULL;
char *gPATH = NULL;
char *gID = NULL;

int main(int argc, char* argv[]) {
  char opt;
  char *port = gLISTEN_PORT;

  while ((opt = getopt(argc, argv, "p:v")) != -1) {
    switch(opt) {
    case 'p': port = optarg; break;
    case 'v': gVERBOSE = 1; break;// print output
    case '?': printf("unknown option: %c\n", optopt); break;
    }
  }

  start_server(port);
  exit(0);
}

void
parse_message(int length) {
  char *ptr = gBUF;

  gPAYLOAD = strpbrk(ptr, "\r\n");
  *gPAYLOAD++ = '\0';
  while (*gPAYLOAD == '\r' || *gPAYLOAD == '\n') gPAYLOAD++;
  gPAYLOAD_SIZE = strlen(gPAYLOAD);

  gMETHOD = strtok_r(gBUF,  " ", &ptr);

  gURI = strtok_r(NULL, " ", &ptr);
  if (gVERBOSE) fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", gMETHOD, gURI ? gURI : "EMPTY");
  if (gURI == NULL) {
    sendp("BAD");
    return;
  }

  gID = gURI;
  if (*gID == '/') gID++;
  if ((ptr = strchr(gID, '/'))) {
    *ptr = '\0';
    gPATH = ptr + 1;
  }

  int ok = 0;
  if ((strlen(gID) == 15 || strlen(gID) == 17) && strspn(gID, "1234567890") == strlen(gID)) ok = 1;
  else if (strlen(gID) == 17) {
    int x = 0;
    char *ptr = gID;
    while (*ptr) if (*ptr++ == ':') x++;
    if (x == 5) ok = 1;
  }

  if (!ok) {
    gID = "";
    gPATH = gURI;
  }

  char *remoteid = ascii_peer();

  gLOGSTRING = stracat(gLOGSTRING, "remoteid is ");
  gLOGSTRING = stracat(gLOGSTRING, remoteid);
  gLOGSTRING = stracat(gLOGSTRING, "id is ");
  gLOGSTRING = stracat(gLOGSTRING, gID);
  if (gPATH && strlen(gPATH)) {
    gLOGSTRING = stracat(gLOGSTRING, " path is ");
    gLOGSTRING = stracat(gLOGSTRING, gPATH);
  } else gLOGSTRING = stracat(gLOGSTRING, " empty path ");
  if (gPAYLOAD && strlen(gPAYLOAD)) {
    gLOGSTRING = stracat(gLOGSTRING, " payload is ");
    gLOGSTRING = stracat(gLOGSTRING, gPAYLOAD);
  } else gLOGSTRING = stracat(gLOGSTRING, " empty payload ");
  if (gVERBOSE) {
    fprintf(stderr, "remoteid is %s; ", remoteid);
    fprintf(stderr, "id is %s; ", gID);
    if (gPATH && strlen(gPATH)) fprintf(stderr, "path is %s; ", gPATH);
    else fprintf(stderr, "empty path; ");
    if (gPAYLOAD && strlen(gPAYLOAD)) fprintf(stderr, "payload is %s;", gPAYLOAD);
    else fprintf(stderr, "no payload; ");
    fprintf(stderr, "\n");
  }
}

void
start_server(const char *port) {
  if (gVERBOSE) fprintf(stderr, "UDP server started on 127.0.0.1 port %s\n", port);

  // getaddrinfo for host
  struct addrinfo hints, *res;
  memset (&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo( NULL, port, &hints, &res) != 0) {
    perror ("getaddrinfo() error");
    return;
  }

  // socket and bind
  struct addrinfo *p;
  for (p = res; p != NULL; p = p->ai_next) {
    gLISTENFD = socket (p->ai_family, p->ai_socktype, 0);
    if (gLISTENFD == -1) continue;
    int option = 1;
    //    setsockopt(gLISTENFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    //    setsockopt(gLISTENFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    setsockopt(gLISTENFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
    if (bind(gLISTENFD, p->ai_addr, p->ai_addrlen) == 0) break;
  }
  if (p == NULL) {
    perror ("socket() or bind()");
    freeaddrinfo(res);
    return;
  }

  freeaddrinfo(res);

  // Ignore SIGCHLD to avoid zombie threads
  signal(SIGCHLD,SIG_IGN);

  sem_t *sem;
  sem = mmap(NULL, sizeof sem, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  sem_init(sem, 1, 0);

  while (1) {
    socklen_t addr_len = sizeof gTHEIR_ADDR;
    if (recvfrom(gLISTENFD, gBUF, sizeof gBUF, MSG_PEEK, (struct sockaddr_in *) &gTHEIR_ADDR, &addr_len) > 0) {
      if (fork() == 0) {
	int rcvd = read_connx(gLISTENFD);
	if (rcvd) {
	  sem_post(sem);
	  parse_message(rcvd);
	  route();
	}
	exit(0);
      }
    }
    sem_wait(sem);
  }
}

void
route() {
  if (strcmp(gMETHOD, "GET") == 0 && strcasecmp(gPATH, "/checktest") == 0) {
    sendp("checktest\n");
    return;
  }

  if (strcmp(gMETHOD, "GET") == 0 && strcasecmp(gPATH, "params") == 0) {
    send_params(gID);
    return;
  }

  if (strcmp(gMETHOD, "POST") == 0 && strcasecmp(gPATH, "/testpost") == 0) {
    sendp("testpost\n");
    return;
  }

  if (strcmp(gMETHOD, "POST") == 0) {
    fprintf(stderr, "POST data is %s\n", gPAYLOAD);
    sendp("posted");
    return;
  }

  if ((strcmp(gMETHOD, "PUT") == 0  || strcmp(gMETHOD, "POST") == 0) &&
      strcasecmp(gPATH, "params") == 0) {
    fprintf(stderr, "%s param data is (%d) %s\n", gMETHOD, gPAYLOAD_SIZE, gPAYLOAD);
    if (recv_params(gID)) sendp("201 New params accepted");
    return;
  }

  sendp("UFO");
  return;
}

int
read_connx(int fd) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  fprintf(stderr, "%d%02d%02d %02d:%02d:%02d ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

  fprintf(stderr, "[%d] ", getpid());

  char *ptr = ascii_peer();
  fprintf(stderr, "(%s) ", ptr ? ptr : "no peer!");

  int rcvd = recvfrom(fd, gBUF, sizeof gBUF, 0, NULL, NULL);

  int clientfd;
  if (rcvd < 0)    // receive error
    fprintf(stderr,("  read/recv error\n"));
  else if (rcvd == 0)    // receive socket closed
    fprintf(stderr,"   Client disconnected upexpectedly.\n");
  else {   // message received
    gBUF[rcvd+1] = '\0';
  }
  fprintf(stderr, " [%d] ", rcvd);
  return rcvd;
}

char *
ascii_peer() {
  static char abuf[INET6_ADDRSTRLEN] = "";
  //  if (gTHEIR_ADDR.ss_family == AF_INET) {
    struct in_addr addr = ((struct sockaddr_in *)&gTHEIR_ADDR)->sin_addr;
    if (!inet_ntop(AF_INET, &addr, abuf, INET6_ADDRSTRLEN)) strcpy(abuf, "NO ADDR");
    //  } else if (gTHEIR_ADDR.ss_family == AF_INET6) {
    //    struct in6_addr addr6 = ((struct sockaddr_in6 *)&gTHEIR_ADDR)->sin6_addr;
    //    if (!inet_ntop(AF_INET6, &addr6, abuf, INET6_ADDRSTRLEN)) strcpy(abuf, "NO ADDR");
    //  }
  return abuf;
}

int
recv_params(char *id) {
  if (id == NULL || strlen(id) == 0) return 0;

  if (gPAYLOAD == NULL || strlen(gPAYLOAD) == 0) return 0;

  cJSON *params = cJSON_Parse(gPAYLOAD);
  if (params == NULL) {
    sendp("500 - Bad json");
    return 0;
  }
  cJSON_Delete(params);

  char fname[PATH_MAX];
  snprintf(fname, PATH_MAX, "%s/%s.json", PARAMDIR, id);

  struct stat sb;
  char newfile[PATH_MAX];
  if (stat(fname, &sb) == 0) {
    do {
      time_t now = time(NULL);
      snprintf(newfile, PATH_MAX, "%s/%s.json.%ld", PARAMDIR, id, now);
    } while (stat(newfile, &sb) == 0);
    if (rename(fname, newfile)) {
      sendp("500 - server error");
      return 0;
    }
  }

  FILE *fp = fopen(fname, "w");
  if (fp == NULL) {
    sendp("502 - FILE ERROR");
    rename(newfile, fname);
    return 0;
  }

  size_t c = fwrite(gPAYLOAD, 1, gPAYLOAD_SIZE, fp);
  if (c != gPAYLOAD_SIZE) {
    if (gVERBOSE) fprintf(stderr, "short write %lu not %u on %s\n", c, gPAYLOAD_SIZE, fname);
    fclose(fp);
    rename(newfile, fname);
    sendp("502 - FILE ERROR");
    return 0;
  }
  fclose(fp);
  return 1;
}

int
send_params(char *id) {
  if (id == NULL || strlen(id) == 0) return 0;

  char fname[PATH_MAX];
  snprintf(fname, PATH_MAX, "%s/%s.json", PARAMDIR, id);
  if (fname == NULL) return 0;

  struct stat sb;
  if (stat(fname, &sb) < 0) {
    sendp("401 - NO PARAMS");
    return 0;
  }

  char *buffer = malloc(sb.st_size + 1);
  if (buffer == NULL) {
    if (gVERBOSE) fprintf(stderr, "can't malloc(%ld) for conf %s\n", sb.st_size, fname);
    sendp("402 - SERVER ERROR");
    return 0;
  }

  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    if (buffer) free(buffer);
    sendp("402 - FILE ERROR");
    return 0;
  }

  size_t c = fread(buffer, 1, sb.st_size, fp);
  if (c != sb.st_size) {
    if (gVERBOSE) fprintf(stderr, "short read %lu not %lu on %s\n", c, sb.st_size, fname);
    if (buffer) free(buffer);
    fclose(fp);
    sendp("402 - FILE ERROR");
    return 0;
  }
  fclose(fp);

  if (buffer[c-1] != '\n') {
    buffer[c] = '\n';
    buffer[c+1] = '\0';
  } else buffer[c] = '\0';

  cJSON *params = cJSON_Parse(buffer);
  if (buffer) free(buffer);
  char *new = cJSON_PrintStruct(params, NULL, 1);
  new = stracat(new, "\n");
  cJSON_Delete(params);
  sendto(gLISTENFD, new, strlen(new), 0, (struct sockaddr_in *) &gTHEIR_ADDR, sizeof(struct sockaddr_in));
  if (new) free(new);
  return 1;
}

void
sendp(char *pMessage, ...) {
  va_list ap;
  va_start(ap, pMessage);

  char buffer[64 * 1024];
  int c = vsnprintf(buffer, sizeof buffer, pMessage, ap);
  int a = sendto(gLISTENFD, buffer, c, 0, (struct sockaddr_in *) &gTHEIR_ADDR, sizeof(struct sockaddr_in));

  va_end(ap);

  return;
}

void
debug(char *pMessage, ...) {
  FILE *f = fopen("iotserver.debug", "a");
  if (f == NULL) return;

  va_list ap;
  va_start(ap, pMessage);
  vfprintf(f, pMessage, ap);
  va_end(ap);

  fclose(f);
  return;
}

char *
stracat(char *dest, char *src) {
  if (src == NULL) return dest;

  int slen = strlen(src);
  if (dest == NULL) {
    char *new = (char *)malloc(slen + 1);
    if (new == NULL) return NULL;
    memcpy(new, src, slen + 1);
    return new;
  }

  int dlen = strlen(dest);
  char *new = (char *)malloc(dlen + slen + 1);
  if (new == NULL) return NULL;
  memcpy(new, dest, dlen);
  memcpy(new + dlen, src, slen + 1);
  free(dest);
  return new;
}
