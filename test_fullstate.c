#include <getopt.h>

#include "main.h"


int gVERBOSE = 0;
char *gLOGSTRING = NULL;


/* int gLISTENFD; */
/* struct sockaddr_in gTHEIR_ADDR; */
/* char *gPAYLOAD = NULL; */
/* int gPAYLOAD_SIZE = 0; */
/* char *gMETHOD = NULL; */
/* char *gURI = NULL; */
/* char *gPATH = NULL; */
/* char *gID = NULL; */

//char *gLISTEN_PORT = "57573";

char gBUF[64 * 1024] = "";

char* TEST_FILE_NAME = "fullstates";



int
test_parse(char *id) {
  fprintf(stderr,"BEGINNING\n");
  if (id == NULL || strlen(id) == 0) return 0;

  char fname[PATH_MAX];
  // This is done just to remove warning!
  sprintf(fname,"%s","spudboy");
  snprintf(fname, PATH_MAX, "%s/%s.json", PARAMDIR, id);
  fprintf(stderr,"AAA %s\n",fname);
  if (fname == NULL) {
    fprintf(stderr,"fname %s: not founds\n",fname);
    return 0;
  }

  fprintf(stderr,"BBB\n");
  struct stat sb;
  if (stat(fname, &sb) < 0) {
    //    sendp("401 - NO PARAMS");
    fprintf(stderr,"401 - NO PARAMS\n");
    return 0;
  }

  fprintf(stderr,"CCC\n");
  char *buffer = malloc(sb.st_size + 1);
  if (buffer == NULL) {
    if (gVERBOSE) fprintf(stderr, "can't malloc(%ld) for conf %s\n", sb.st_size, fname);
    fprintf(stderr,"402 - SERVER ERROR\n");
    //sendp("402 - SERVER ERROR");
    return 0;
  }

  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    if (buffer) free(buffer);
    fprintf(stderr,"402 - FILE ERROR\n");
    //    sendp("402 - FILE ERROR");
    return 0;
  }

  size_t c = fread(buffer, 1, sb.st_size, fp);
  if (c != sb.st_size) {
    if (gVERBOSE) fprintf(stderr, "short read %lu not %lu on %s\n", c, sb.st_size, fname);
    if (buffer) free(buffer);
    fclose(fp);
    fprintf(stderr,"402 - FILE ERROR\n");
    //    sendp("402 - FILE ERROR");
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
  //  sendto(gLISTENFD, new, strlen(new), 0, (struct sockaddr_in *) &gTHEIR_ADDR, sizeof(struct sockaddr_in));
  fprintf(stderr,"HERE IS THE PARASED FILE:\n%s",
          new);
  if (new) free(new);
  return 1;
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


int main(int argc, char* argv[]) {
  char opt;
  //  char *port = gLISTEN_PORT;

  while ((opt = getopt(argc, argv, "p:v")) != -1) {
    switch(opt) {
      //    case 'p': port = optarg; break;
    case 'v': gVERBOSE = 1; break;// print output
    case '?': printf("unknown option: %c\n", optopt); break;
    }
  }

  gVERBOSE = 1;
  //  start_server(port);
  test_parse(TEST_FILE_NAME);
  exit(0);
}
