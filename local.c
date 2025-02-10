#include "local.h"


// int gVERBOSE = 0;
// HACK! Trying to understand why I can't read my full file!
int gVERBOSE = 1;
char *gLOGSTRING = NULL;

int main(int argc, char* argv[]) {
  char opt;
  
  while ((opt = getopt(argc, argv, "v")) != -1) { 
    switch(opt) { 
    case 'v': gVERBOSE = 1; break;// print output
    case '?': printf("unknown option: %c\n", optopt); break; 
    } 
  } 

  if (optind == argc) {
    fprintf(stderr, "Need mac and type\n");
    exit(1);
  }

  start_reader(argv[optind], argv[optind + 1]);
  exit(0);
}

void
start_reader(char *mac, char *type) {
  if (!mac) return;
  if (!type) return;

  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "DATA/%s-%s", mac, type);
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "cant open file");
    return;
  }

  fprintf(stderr, "Processing %s\n", filename);

  char *line = NULL;
  size_t c = 0;
  char buffer[64*1024];
  while (getline(&line, &c, fp) >= 0) {
    fprintf(stderr, ".");
    char *ptr = strchr(line, '}');
    if (ptr) {
      *(++ptr) = '\0';
      strcat(buffer, line);
      relay_data(mac, buffer, type, 1);
      fprintf(stderr, "+");
      strcpy(buffer, ptr);
    } else strcat(buffer, line);
  }
  fprintf(stderr, "\n");
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

