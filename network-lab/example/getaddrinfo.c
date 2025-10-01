#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main() {
  const char *domain = "example.com";
  struct addrinfo hints, *res, *p;
  int status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // IPv4 only
  hints.ai_socktype = SOCK_STREAM; // TCP

  if ((status = getaddrinfo(domain, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return 1;
  }

  printf("IPv4 addresses for %s:\n", domain);

  for (p = res; p != NULL; p = p->ai_next) {
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
    char ipstr[INET_ADDRSTRLEN];

    // convert to string
    inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof ipstr);

    printf("  %s\n", ipstr);
  }

  freeaddrinfo(res);

  return 0;
}
