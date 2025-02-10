#include "main.h"

//#define RCV_PARAMS


// int gVERBOSE = 0;
// HACK! Trying to understand why I can't read my full file!
int gVERBOSE = 1;

//char *gLOGSTRING = NULL;

char *gLISTEN_PORT = "57575";

int gLISTENFD;

sem_t *gSEM;

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

int
send_params(char *id, char *type, struct sockaddr_in their_addr) {
  if (id == NULL || strlen(id) == 0) return 0;
  
  char fname[PATH_MAX];
  if (type) snprintf(fname, PATH_MAX, "%s/%s-%s.json", PARAMDIR, id, type);
  else snprintf(fname, PATH_MAX, "%s/%s.json", PARAMDIR, id);
  if (fname == NULL) return 0;

  struct stat sb;
  if (stat(fname, &sb) < 0) {
    sendp(their_addr, "401 - NO PARAMS");
    return 0;
  }

  char *buffer = malloc(sb.st_size + 1);
  if (buffer == NULL) {
    if (gVERBOSE) fprintf(stderr, "can't malloc(%ld) for conf %s\n", sb.st_size, fname);
    sendp(their_addr, "402 - SERVER ERROR");
    return 0;
  }
    
  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    if (buffer) free(buffer);
    sendp(their_addr, "402 - FILE ERROR");
    return 0;
  }

  size_t c = fread(buffer, 1, sb.st_size, fp);
  if (c != sb.st_size) {
    if (gVERBOSE) fprintf(stderr, "short read %lu not %lu on %s\n", c, sb.st_size, fname);
    if (buffer) free(buffer);
    fclose(fp);
    sendp(their_addr, "402 - FILE ERROR");
    return 0;
  }
  fclose(fp);

  if (buffer[c-1] != '\n') {
    buffer[c] = '\n';
    buffer[c+1] = '\0';
  } else buffer[c] = '\0';
  
  char *startptr = buffer;
  if (strncmp(buffer, "{ \"C8YSID\": ", 12) == 0) {
    startptr = strchr(buffer, '\n');
    startptr++;
  }

  cJSON *params = cJSON_Parse(startptr);
  if (buffer) free(buffer);

  char *new = FlattenJSON(params, "States", 1);
  new = stracat(new, "\n");
  cJSON_Delete(params);

  sendto(gLISTENFD, new, strlen(new), 0, (struct sockaddr_in *) &their_addr, sizeof(struct sockaddr_in));

  fprintf(stderr,"HERE IS THE PARASED FILE:\n%s", new);
  if (new) free(new);
  return 1;
}

struct uri_struct {
  char *method;
  char *path;
  char *id;
  char *type;
};

void
route(struct uri_struct uri, char *payload, struct sockaddr_in their_addr) {
  if (strcmp(uri.method, "GET") == 0 && strcasecmp(uri.path, "checktest") == 0) {
    for(int i = 1; i < 600; i++) {
      char msg[20];
      fprintf(stderr, "send %d\n", i);
      sprintf(msg, "CheckTest %d\n", i*90);
      sendp(their_addr, msg);
      sleep(90);
    }
    return;
  }

  if (strcmp(uri.method, "GET") == 0 && strcasecmp(uri.path, "params") == 0) {
    send_params(uri.id, uri.type, their_addr);
    return;
  }

  if (strcmp(uri.method, "POST") == 0 && strcasecmp(uri.path, "/testpost") == 0) {
    sendp(their_addr, "testpost\n");
    return;
  }

#ifdef RCV_PARAMS
  if ((strcmp(uri.method, "PUT") == 0  || strcmp(uri.method, "POST") == 0) && strcasecmp(uri.path, "params") == 0) {
    fprintf(stderr, "%s param data is (%d) %s\n", uri.method, strlen(payload), payload);
    if (recv_params(uri.id)) sendp(their_addr, "201 New params accepted");
    return;
  }
#endif
  
  if (strcmp(uri.method, "POST") == 0 || strcmp(uri.method, "PUT") == 0) {
    sem_wait(gSEM);
    fprintf(stderr, "POST data is %s\n", payload);
    char fname[PATH_MAX];
    if (uri.type) snprintf(fname, sizeof fname, "DATA/%s-%s", uri.id, uri.type);
    else snprintf(fname, sizeof fname, "DATA/%s", uri.id);

    FILE *fp = fopen(fname, "a");
    if (fp == NULL) {
      sendp(their_addr, "402 post failed - file error");
      return;
    }
    fprintf(fp, "%s\n", payload);
    fclose(fp);
    sem_post(gSEM);

#ifdef CUMULOCITY
    int r = relay_data(uri.id, payload, uri.type, 0);
#endif
    sendp(their_addr, "posted");
    return;
  }

  sendp(their_addr, "UFO");
  return;
}

void
handle_message(int rcvd, char *buffer, struct sockaddr_in their_addr) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  fprintf(stderr, "%d/%02d/%02d %02d:%02d:%02d ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  
  //  fprintf(stderr, "[%d] ", getpid());
  
  char remoteid[INET6_ADDRSTRLEN] = "";
  struct in_addr addr = ((struct sockaddr_in *) & their_addr)->sin_addr;
  if (!inet_ntop(AF_INET, &addr, remoteid, INET6_ADDRSTRLEN))
    strcpy(remoteid, "NO ADDR");

  fprintf(stderr, "(%s) ", remoteid ? remoteid : "no peer!");
  
  fprintf(stderr, " [%d] ", rcvd);

  char *ptr = buffer;

  char *payload = strpbrk(ptr, "\r\n");
  if (payload) {
    *payload++ = '\0';
    while (*payload == '\r' || *payload == '\n') payload++;
  }

  struct uri_struct uri;

  uri.method = strtok_r(buffer,  " ", &ptr);

  char *uriL = strtok_r(NULL, " ", &ptr);
  if (gVERBOSE) fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", uri.method, uriL ? uriL : "EMPTY");
  if (uriL == NULL) {
    sendp(their_addr, "BAD URI");
    return;
  }

  uri.id = uriL;
  if (*uri.id == '/') uri.id++;
  if (ptr = strchr(uri.id, '/')) {
    *ptr = '\0';
    uri.path = ptr + 1;
  }

  int ok = 0;
  if ((strlen(uri.id) == 15 || strlen(uri.id) == 17) && strspn(uri.id, "1234567890") == strlen(uri.id)) ok = 1;
  else if (strlen(uri.id) == 17) {
    int x = 0;
    char *ptr = uri.id;
    while (*ptr) if (*ptr++ == ':') x++;
    if (x == 5) ok = 1;
  }
  
  if (!ok) {
    uri.id = "";
    uri.path = uriL;
  }

  char *type;
  if (ptr = strchr(uri.path, '/')) {
    *ptr = '\0';
    uri.type = uri.path;
    uri.path = ptr + 1;
  }

  if (!uri.type || strlen(uri.type) == 0) {
    sendp(their_addr, "BAD type endpoint");
    return;
  }

  if (uri.type && strcasecmp(uri.type, "ODECS") == 0) uri.type = "OEDCS";

#if 0
  gLOGSTRING = stracat(gLOGSTRING, "remoteid is ");
  gLOGSTRING = stracat(gLOGSTRING, remoteid);
  gLOGSTRING = stracat(gLOGSTRING, "id is ");
  gLOGSTRING = stracat(gLOGSTRING, uri.id);
  if (uri.path && strlen(uri.path)) {
    gLOGSTRING = stracat(gLOGSTRING, " path is ");
    gLOGSTRING = stracat(gLOGSTRING, uri.path);
  } else gLOGSTRING = stracat(gLOGSTRING, " empty path ");
  if (payload && strlen(payload)) {
    gLOGSTRING = stracat(gLOGSTRING, " payload is ");
    gLOGSTRING = stracat(gLOGSTRING, payload);
  } else gLOGSTRING = stracat(gLOGSTRING, " empty payload ");
#endif
  
  if (gVERBOSE || 1) {
    fprintf(stderr, "remoteid is %s; ", remoteid);
    fprintf(stderr, "id is %s; ", uri.id);
    if (uri.path && strlen(uri.path)) fprintf(stderr, "path is %s ", uri.path);
    else fprintf(stderr, "empty path; ");
    if (payload && strlen(payload)) fprintf(stderr, "payload is %s", payload);
    else fprintf(stderr, "no payload; ");
    fprintf(stderr, "\n");
  }

  route(uri, payload, their_addr);
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

  gSEM = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
  sem_init(gSEM, 1, 1);
    
  while (1) {
    struct sockaddr_in their_addr;
    socklen_t addr_len = sizeof their_addr;
    char buffer[64*1024];
    int rcvd = recvfrom(gLISTENFD, buffer, sizeof buffer, 0, &their_addr, &addr_len);
    if (rcvd < 0) {
      fprintf(stderr,("  read/recv error\n"));
      continue;
    }
    if (rcvd == 0) {      // receive socket closed
      fprintf(stderr,"   Client disconnected upexpectedly.\n");
      continue;
    }

    buffer[rcvd] = '\0';

    switch(fork()) {
    case 0: // child
      handle_message(rcvd, buffer, their_addr);
      exit(0);
    case -1: //failed
    default: //parent
    }
  }
}

#ifdef RCV_PARAMS
int
recv_params(struct sockaddr_in their_addr, char *id, char *payload) {
  if (id == NULL || strlen(id) == 0) return 0;

  if (payload == NULL || strlen(payload) == 0) return 0;

  cJSON *params = cJSON_Parse(payload);
  if (params == NULL) {
    sendp(their_addr, "500 - Bad json");
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
      sendp(their_addr, "500 - server error");
      return 0;
    }
  }

  FILE *fp = fopen(fname, "w");
  if (fp == NULL) {
    sendp(their_addr, "502 - FILE ERROR");
    rename(newfile, fname);
    return 0;
  }

  size_t c = fwrite(payload, 1, payload_size, fp);
  if (c != payload_size) {
    if (gVERBOSE) fprintf(stderr, "short write %lu not %u on %s\n", c, payload_size, fname);
    fclose(fp);
    rename(newfile, fname);
    sendp(their_addr, "502 - FILE ERROR");
    return 0;
  }
  fclose(fp);
  return 1;
}
#endif

char *
FlattenJSON(cJSON *item, char *label, int values) {
  static int level = 0;
  static char *outbuffer = NULL;
  static char *prefix = NULL;

  int element_count = 0;

  if (level == 0) {
    if (outbuffer) free(outbuffer);
    outbuffer = NULL;
  }

  char *tmp = NULL;
  switch ((item->type) & 0xFF) {
  case cJSON_NULL: tmp = strdup("NULL"); break;
  case cJSON_False: tmp = strdup("FALSE"); break;
  case cJSON_True: tmp = strdup("TRUE"); break;

  case cJSON_Number:
    double d = item->valuedouble;
    if (isnan(d) || isinf(d))  tmp = strdup("INVNUM");
    else {
      char dtemp[50];
      snprintf(dtemp, sizeof dtemp, "%1.15g", d);
      tmp = strdup(dtemp);
    }
    break;

  case cJSON_Raw:
    if (item->valuestring == NULL) tmp = strdup("RAWNULL");
    else tmp = strdup(item->valuestring);
    break;

  case cJSON_String:
    tmp = strdup(item->valuestring);
    break;

  case cJSON_Array:
    cJSON *current_element = item->child;
    while (current_element) {
      element_count++;
      int olen = 0; if (outbuffer) olen = strlen(outbuffer);
      if (olen) {
	outbuffer = realloc(outbuffer, olen + 10);
	//	char n[50]; snprintf(n, sizeof n, "%d", level);
	//	strcat(outbuffer, n);
	if (level == 1) strcat(outbuffer, "[");
	strcat(outbuffer, "[");
	strcat(outbuffer, " ");
      }
      level++;
      FlattenJSON(current_element, label, values);
      level--;
      if (current_element->next) {
        int olen = 0; if (outbuffer) olen = strlen(outbuffer);
	outbuffer = realloc(outbuffer, olen + 3);
	strcat(outbuffer, "\n");
      } 
      current_element = current_element->next;
    }
    break;

  case cJSON_Object:
    cJSON *current_object = item->child;
    while (current_object) {
      if (current_object->string && strcasecmp(current_object->string, label)) {
	if (!prefix) {
	  prefix = malloc(strlen(current_object->string) + 2);
	  strcpy(prefix, "");
	} else
	  prefix = realloc(prefix, strlen(prefix) + 1 + strlen(current_object->string) + 2);
	strcat(prefix, current_object->string);
	strcat(prefix, ".");
      }
      level++;
      FlattenJSON(current_object, label, values);
      level--;
      char *ptr = strrchr(prefix, '.');
      if (ptr) *ptr = '\0';
      ptr = strrchr(prefix, '.');
      if (ptr) *++ptr = '\0';
      else strcpy(prefix, "");
      if (current_object->next) {
	int olen = 0; if (outbuffer) olen = strlen(outbuffer);
	outbuffer = realloc(outbuffer, olen + 5);
	strcat(outbuffer, "\n");
      }
      current_object = current_object->next;
    }
    break;
  }

  if (tmp) {
    int olen = 0;
    if (outbuffer) olen = strlen(outbuffer);
    int plen = 0;
    if (prefix) plen = strlen(prefix);
    int tlen = 0; if (tmp) tlen = strlen(tmp);
    if (!outbuffer) {
      outbuffer = malloc(plen + 1 + tlen + 5);
      strcpy(outbuffer, "");
    } else
      outbuffer = realloc(outbuffer, olen + 1 + plen + 1 + tlen + 5);
    if (plen) {
      strcat(outbuffer, prefix);
      strcat(outbuffer, " ");
    }
    if (tlen && values) strcat(outbuffer, tmp);
    if (tmp) free(tmp);
  }
  if (level == 0) {
    if (outbuffer) return strdup(outbuffer);
    return strdup("");
  }
}

void
sendp(struct sockaddr_in their_addr, char *pMessage, ...) {
  va_list ap;
  va_start(ap, pMessage);

  char buffer[64 * 1024];
  int c = vsnprintf(buffer, sizeof buffer, pMessage, ap);
  int a = sendto(gLISTENFD, buffer, c, 0, (struct sockaddr_in *) &their_addr, sizeof(struct sockaddr_in));
    
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

