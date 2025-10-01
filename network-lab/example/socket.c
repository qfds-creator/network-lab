#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// This program establish a TCP connection to 127.0.0.1:1234, and send
// "HELO\r\n" to remote, print everything recieved to stdout.
//
// Run `nc -l 1234` in the same computer, and run this program afterwards.
int main() {
  int sockfd;
  struct sockaddr_in serv_addr;
  char sendbuf[] = "HELO\r\n";
  char recvbuf[BUFFER_SIZE];
  ssize_t sent_bytes, recv_bytes;

  // Create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket creation failed");
    return 1;
  }

  // Initialize server address structure
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;   // IPv4
  serv_addr.sin_port = htons(1234); // Set port

  // Set address
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    perror("inet_pton error");
    close(sockfd);
    return 1;
  }

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect failed");
    close(sockfd);
    return 1;
  }

  // Send data to the server
  sent_bytes = send(sockfd, sendbuf, strlen(sendbuf), 0);
  if (sent_bytes < 0) {
    perror("send failed");
    close(sockfd);
    return 1;
  }

  // Receive data from the server and print it
  while ((recv_bytes = recv(sockfd, recvbuf, BUFFER_SIZE, 0)) > 0) {
    if (write(STDOUT_FILENO, recvbuf, recv_bytes) < 0) {
      perror("failed to write to stdout");
      close(sockfd);
      return 1;
    }
  }

  if (recv_bytes < 0) {
    perror("recv failed");
    close(sockfd);
    return 1;
  }

  // Close the socket
  close(sockfd);

  return 0;
}
