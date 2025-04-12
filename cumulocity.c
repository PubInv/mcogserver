#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
//#include <openssl/err.h>
#include <unistd.h>
#include <errno.h>
#include "secrets.h"

#include "main.h"
#define sendproperties

// Celsius °C
// Fahrenheit °F

//extern int C8Y_RELAY;
//extern char *C8Y_AUTH;
//extern char *C8Y_HOST;
//extern char *gPAYLOAD;

static int create_socket(char *hostname, int port);
static int is_ssl(char *url);
static char *get_hostname(char *url);
static int get_port(char *url);
static char *getInternalDeviceId(char *externalId);
static int send_request(char *request);
static char *response_body(char *response);
static int response_code(char *response);

static SSL_CTX *gCTX;

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};

#include <stdint.h>
char *
base64_encode(const unsigned char *data) {
  size_t input_length = strlen(data);
  size_t output_length = 4 * ((input_length + 2) / 3);

  static char *encoded_data = NULL;

  if (encoded_data) free(encoded_data);
  encoded_data = malloc(output_length + 1);
  if (encoded_data == NULL) return NULL;

  for (int i = 0, j = 0; i < input_length;) {
    uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_length % 3]; i++) encoded_data[output_length - 1 - i] = '=';
  encoded_data[output_length] = '\0';

  return encoded_data;
}

int
create_socket(char *hostname, int port) {
  if (gVERBOSE) fprintf(stderr, "create socket to %s %d\n", hostname, port);

  struct hostent *host;
  if ((host = gethostbyname(hostname)) == NULL ) {
    if (gVERBOSE) fprintf(stderr, "Error: Cannot resolve hostname %s.\n",  hostname);
    return -1;
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in dest_addr;
  dest_addr.sin_family=AF_INET;
  dest_addr.sin_port=htons(port);
  dest_addr.sin_addr.s_addr = *(long*) (host->h_addr);

  memset(&(dest_addr.sin_zero), '\0', 8); // Zero rest of the struct

  if (connect(sockfd, (struct sockaddr_in *) &dest_addr, sizeof(struct sockaddr_in)) == -1 ) {
    char *tmp_ptr = inet_ntoa(dest_addr.sin_addr);
    if (gVERBOSE) fprintf(stderr, "Error: Cannot connect to host %s [%s] on port %d.\n", hostname, tmp_ptr, port);
    return -1;
  }

  if (sockfd <= 0) {
    if (gVERBOSE) fprintf(stderr, "couldn't create socket\n");
    return -2;
  }
  return sockfd;
}

int
is_ssl(char *url) {
  if (strncasecmp(url, "https", 5) == 0) return 1;
  return 0;
}

char *
get_hostname(char *url) {
  static char *hostname = NULL;
  if (hostname) free(hostname);
  hostname = strdup(url);

  char *ptr = strstr(hostname, "://");
  if (ptr == NULL) return NULL;
  ptr += 3;
  char *ptr1 = strchr(ptr, ':');
  if (ptr1) *ptr1 = '\0';
  ptr1 = strchr(ptr, '/');
  if (ptr1) *ptr1 = '\0';
  return ptr;
}

int
get_port(char *url) {
  char *ptr = strstr(url, "://");
  if (ptr == NULL) return -1;
  ptr += 3;
  if ((ptr = strchr(ptr, ':')) == NULL) return 0;
  errno = 0;
  unsigned long r = strtoul(ptr+1, NULL, 10);
  if (errno) return -1;
  if (r < 1 || r > 65535) return 0;
  return r;
}

int
send_request(char *request) {
  if (! C8Y_RELAY) return 0;
  if (request == NULL) {
    if (gVERBOSE) fprintf(stderr, "  No request\n");
    return 0;
  }

  int usessl = is_ssl(C8Y_HOST);
  char *hostname = get_hostname(C8Y_HOST);
  if (!hostname) return 0;
  int port = get_port(C8Y_HOST);
  if (port < 0) return 0;
  if (port == 0) {
    port = 80;
    if (usessl) port = 443;
  }

  int socket = create_socket(hostname, port);
  if (socket < 0) return -1;

  SSL *ssl = NULL;
  SSL_CTX *ctx;
  if (usessl) {
    if (SSL_library_init() < 0) {
      if (gVERBOSE) fprintf(stderr, "can't init ssl\n");
      close(socket);
      return -3;
    }

    //    SSL_load_error_strings(); // bring in error messages
    const SSL_METHOD *method = SSLv23_client_method();
    if ((ctx = SSL_CTX_new(method)) == NULL) {
      if (gVERBOSE) fprintf(stderr, "can't create ssl context\n");
      close(socket);
      return -4;
    }
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);  //disable sslv2???

    ssl = SSL_new(ctx); // create new ssl state
    if (!ssl) {
      if (gVERBOSE) fprintf(stderr, "Can't create ssl structure\n");
      close(socket);
      SSL_CTX_free(ctx);
      return -1;
    }
    
    SSL_set_fd(ssl, socket); // attach the socket

    if (SSL_connect(ssl) != 1) { // connect
      if (gVERBOSE) fprintf(stderr, "cant build ssl");
      SSL_free(ssl);
      close(socket);
      SSL_CTX_free(ctx);
      return -5;
    } else if (gVERBOSE) fprintf(stderr, "enabled ssl\n");
  }

  int r;
  if (usessl) r = SSL_write(ssl, request, strlen(request));
  else r = write(socket, request, strlen(request));
  if (r < strlen(request)) {
    if (gVERBOSE) fprintf(stderr, "short write\n");
    //    gLOGSTRING = stracat(gLOGSTRING, "short write\n");
  } else {
    int bytes = 0;
    r = 0;
    char buf[4096];
    if (usessl) bytes = SSL_read(ssl, buf, sizeof buf);
    else bytes = read(socket, buf, sizeof buf);
    buf[bytes] = '\0';
    if (gVERBOSE) fprintf(stderr, "%s", buf);
    if (strstr(buf, "201 Created")) r = 1;
  }
  if (usessl) SSL_free(ssl);
  close(socket);
  if (usessl) SSL_CTX_free(ctx);
  return r;
}

char *
convert_data_c8y(char *input, char *type, char *id, char **properties, int use_ts) {
  if (!input && strlen(input) == 0) {
    if (gVERBOSE) fprintf(stderr, "  No data\n");
    return NULL;
  }
  if (!type || strlen(type) == 0) {
    fprintf(stderr, "  no type\n");
    return NULL;
  }

  char *ptr = input;
  while (*ptr == ' ') ptr++;

  cJSON *item = cJSON_Parse(ptr);
  if (item == NULL) {
    fprintf(stderr, "Invalid JSON input\n");
    return NULL;
  }
  
  time_t now = 0;
  if (use_ts) {
    cJSON *j = cJSON_GetObjectItem(item, "TimeStamp");
    if (!j || !cJSON_IsNumber(j)) return NULL;
    now = j->valueint;
  } else now = time(NULL);
    
  char nowt[30] = "";
  strftime(nowt, sizeof nowt, "%FT%TZ", gmtime(&now));
  
  char *new = NULL;
  asprintf(&new, "{ \"type\": \"%s\", \"time\": \"%s\", \"source\": {\"id\": \"%s\"}", type, nowt, id);
  new = stracat(new, ", ");

  char *s2hval = NULL;
  if (strcasecmp(type, "Stage2") == 0) {
    cJSON *j = cJSON_GetObjectItem(item, "Stage2Heater");
    if (!j) {
      fprintf(stderr, "Missing Stage2Heater value\n");
      if (new) free(new);
      return NULL;
    }
    s2hval = cJSON_Print(j);
  }

  int foundCE = 0;
  cJSON *current_item = item->child;
  while (current_item) {
    // processed above
    if (strcasecmp(current_item->string, "Stage2Heater") == 0) {
      current_item = current_item->next;
      continue;
    }

    char *value = cJSON_Print(current_item);
    fprintf(stderr, "doing %s (%s)\n", current_item->string, value);

    if (strcasecmp(current_item->string, "CriticalError") == 0) {
      foundCE = 1;
      *properties = stracat(*properties, "CriticalError;");
      char tmp1[strlen(value) + 2]; // allow for ; and NULL
      char *ptr = tmp1;
      char *ptr1 = value;
      while(*ptr1 != '\0') {
	if (*ptr1 != ';' && *ptr1 != '[' && *ptr1 != ']' && *ptr1 != '"') 
	  *ptr++ = *ptr1;
	ptr1++;
      }
      *ptr = '\0';
      //      if (strchr(tmp1, '[') || strchr(tmp1, '{'))
      //	*properties = stracat(*properties, "\"");
      *properties = stracat(*properties, tmp1);
      //      if (strchr(tmp1, '[') || strchr(tmp1, '{'))
      //	*properties = stracat(*properties, "\"");
      *properties = stracat(*properties, ";");
      current_item = current_item->next;
      continue;
    }

    if (strcasecmp(current_item->string, "MachineState") == 0) {
      *properties = stracat(*properties, "MachineState;");
      if (strcmp(value, "0") == 0) *properties = stracat(*properties, "OFF;");
      else if (strcmp(value, "1") == 0) *properties = stracat(*properties, "WARMUP;");
      else if (strcmp(value, "2") == 0) *properties = stracat(*properties, "NormalOperation;");
      else if (strcmp(value, "3") == 0) *properties = stracat(*properties, "CoolDown;");
      else if (strcmp(value, "4") == 0) *properties = stracat(*properties, "CriticalFault;");
      else if (strcmp(value, "5") == 0) *properties = stracat(*properties, "EmergencyShutdown;");
      else if (strcmp(value, "6") == 0) *properties = stracat(*properties, "OFFUserAck;");
      else {
	char tmp1[strlen(value) + 2]; // allow for ; and NULL
	char *ptr = tmp1;
	char *ptr1 = value;
	while(*ptr1 != '\0') {
	  if (*ptr1 == ';') continue;
	  *ptr++ = *ptr1++;
	}
	*ptr++ = ';';
	*ptr = '\0';
	*properties = stracat(*properties, tmp1);
      }
    }

    if (value[0] == '"' && strspn(value, "\".-0123456789") == strlen(value)) {
      char *ptr1 = value;
      char *ptr2 = value;
      while(*ptr1) {
	if (*ptr1 != '"') *ptr2++ = *ptr1;
	ptr1++;
      }
      *ptr2 = '\0';
    } else {
      if (strcasecmp(value, "\"inf\"") == 0) strcpy(value, "99999999");
      else if (strcasecmp(value, "\"off\"") == 0) strcpy(value, "0");
      else if (strcasecmp(value, "\"warmup\"") == 0) strcpy(value, "1");
      else if (strcasecmp(value, "\"normaloperation\"") == 0) strcpy(value, "2");
      else if (strcasecmp(value, "\"cooldown\"") == 0) strcpy(value, "3");
      else if (strcasecmp(value, "\"criticalfault\"") == 0) strcpy(value, "4");
      else if (strcasecmp(value, "\"emergencyshutdown\"") == 0) strcpy(value, "5");
      else if (strcasecmp(value, "\"offuserack\"") == 0) strcpy(value, "6");
      else if (strcasecmp(value, "\"false\"") == 0) strcpy(value, "0");
      else if (strcasecmp(value, "\"true\"") == 0) strcpy(value, "1");
      else if (strcasecmp(value, "\"n/a\"") == 0) strcpy(value, "-1");
      else if (strcasecmp(value, "\"-1.0\"") == 0) strcpy(value, "-1");
      else if (strcasecmp(value, "\"general\"") == 0) strcpy(value, "0");
    }

    if (strspn(value, ".-0123456789") != strlen(value)) { // still invalid measurement skip
      fprintf(stderr, "invalid measurement for %s (%s)\n", current_item->string, value);
      current_item = current_item->next;
      continue;
    }
    
    char *units = "X";
    if (strcasecmp(current_item->string, "TimeStamp") == 0) units = "Time";
    else if (strcasecmp(current_item->string, "MachineState") == 0) units = "State";
    else if (strcasecmp(current_item->string, "TargetC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "SetpointC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "RampC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "MaxStackA") == 0) units = "A";
    else if (strcasecmp(current_item->string, "MaxStackW") == 0) units = "W";
    else if (strcasecmp(current_item->string, "FanPWM") == 0) units = "PWM";
    else if (strcasecmp(current_item->string, "HeaterC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "StackC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "GetterC") == 0) units = "C";
    else if (strcasecmp(current_item->string, "HeaterDutyCycle") == 0) units = "%";
    else if (strcasecmp(current_item->string, "StackA") == 0) units = "A";
    else if (strcasecmp(current_item->string, "StackW") == 0) units = "W";
    else if (strcasecmp(current_item->string, "StackV") == 0) units = "V";
    else if (strcasecmp(current_item->string, "StackOhms") == 0) units = "Ohms";
    else if (strcasecmp(current_item->string, "FanRPM") == 0) units = "RPM";
    
    char *val = NULL;
    if (strcasecmp(type, "OEDCS") == 0) {
      asprintf(&val, "\"%s\": { \"%s\": { \"unit\": \"%s\", \"value\": %s } }%c ", current_item->string, current_item->string, units, value, current_item->next?',':'}');
    } else if (strcasecmp(type, "Stage2") == 0) {
      asprintf(&val, "\"%s_%s\": { \"%s_%s\": { \"unit\": \"%s\", \"value\": %s } }%c ", current_item->string, s2hval, current_item->string, s2hval, units, value, current_item->next?',':'}');
    }
    if (value) free(value);
    new = stracat(new, val);
    if (val) free(val);
    current_item = current_item->next;
  }
  if (! foundCE) *properties = stracat(*properties, "CriticalError;None;");
  if (s2hval) free(s2hval);
  cJSON_Delete(item);

  fprintf(stderr, "props - %s\n", *properties);
  return new;
}

int
relay_data(char *id, char *payload, char *type, int use_ts) { 
  if (id == NULL || strlen(id) == 0) return 0;
  
  char fname[PATH_MAX];
  if (type) snprintf(fname, PATH_MAX, "%s/%s-%s.json", PARAMDIR, id, type);
  else  snprintf(fname, PATH_MAX, "%s/%s.json", PARAMDIR, id);

  FILE *fp = fopen(fname, "r");
  if (fp == NULL) {
    fprintf(stderr, "can't open file %s\n", fname);
    return 0;
  }

  char *line = NULL;
  size_t c = 0;
  char *buff = NULL;
  while (getline(&line, &c, fp) >= 0) {
    buff = stracat(buff, line);
    char *ptr = strchr(buff, '}');
    if (ptr) {
      *(ptr+1) = '\0';
      break;
    }
  }
  fclose(fp);
  if (line) free(line);
  
  cJSON *ident = cJSON_Parse(buff);
  if (ident == NULL) {
    fprintf(stderr, "Invalid JSON input\n");
    if (buff) free(buff);
    return 0;
  }
  if (buff) free(buff);

  char *c8ysid = cJSON_Print(cJSON_GetObjectItem(ident, "C8YSID"));
  cJSON_Delete(ident);
  if (c8ysid == NULL || strlen(c8ysid) == 0) {
    fprintf(stderr, "no c8ysid!\n");
    return 0;
  }

  fprintf(stderr, "C8Y SID is %s type is %s\n", c8ysid, type);
  fprintf(stderr, "Payload is %s\n", payload);
  char *properties;
  char *buffer = convert_data_c8y(payload, type, c8ysid, &properties, use_ts);
  fprintf(stderr, "properties (%s)\n", properties);

  if (!buffer) return 0;
  
  fprintf(stderr, "POSTDATA\n------\n%s\n-----\n", buffer);

  /* send measurement */
  char *request = NULL;
  asprintf(&request, "POST %s/measurement/measurements HTTP/1.1\r\nHost: %s\r\nAuthorization: Basic %s\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", C8Y_HOST, get_hostname(C8Y_HOST), base64_encode(C8Y_AUTH), strlen(buffer), buffer);
  if (buffer) free(buffer);
  buffer = NULL;

  fprintf(stderr, "REQUEST\n------\n%s\n-----\n", request);  

  int r = send_request(request);
  if (request) free(request);
  request = NULL;

  if (r < 0) {
    if (gVERBOSE) fprintf(stderr, "did not send measurements\n");
    if (c8ysid) free(c8ysid);
    return -1;
  }
  
#ifdef sendproperties
  if (properties && strlen(properties)) {
    fprintf(stderr, "send properties -%s\n", properties);

    char *ptr;
    char *prop;
    char *val;
    
    for (ptr = properties; prop = strtok(ptr, ";"); ptr = NULL) {
      val = strtok(NULL, ";");
      asprintf(&request, "PUT %s/inventory/managedObjects/%s HTTP/1.1\r\nHost: %s\r\nAuthorization: Basic %s\r\nX-Cumulocity-Processing-Mode: PERSISTENT\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length:%ld\r\n\r\n{ \"%s\": \"%s\" }\r\n", C8Y_HOST, c8ysid, get_hostname(C8Y_HOST), base64_encode(C8Y_AUTH), 12+strlen(prop)+strlen(val), prop, val);
      fprintf(stderr, "sending request:\n%s\n", request);
      int r = send_request(request);
      if (request) free(request);
      if (r < 0) fprintf(stderr, "did not send property %s\n", prop);
    }
    sleep(5);
    free(properties);
  }
#endif
  
  if (c8ysid) free(c8ysid);
  return 1;
}

char *
response_body(char *response) {
  char *ptr = response;
  
  char *ptr1;
  char *ptr2;
  while ((ptr1 = strchr(ptr, '\r')) || (ptr2 = strchr(ptr, '\n'))) {
    if (ptr1) ptr = ptr1;
    else if (ptr2) ptr = ptr2;
    else return NULL;

    if (*(ptr + 1) == '\0') return NULL;
    if (*ptr == '\n' && *(ptr + 1) == '\n') return(ptr + 2);
    if (*ptr == '\r' && *(ptr + 1) == '\n') {
      if (*(ptr + 2) == '\0') return NULL;
      if (*(ptr + 2) == '\r' && *(ptr + 3) == '\n') return(ptr + 4);
    }
    ptr++;
  }
}    

int
response_code(char *response) {
  char *ptr = response;
  if (strncasecmp(ptr, "HTTP", 4) != 0) return -1;
  while (*ptr++ != ' ');
  while (*(ptr) == ' ') ptr++;
  errno = 0;
  unsigned int code = strtoul(ptr, NULL, 10);
  if (errno) return -1;
  return code;
}

