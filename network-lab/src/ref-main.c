#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_HEADERS 20
#define BUFFER_SIZE 4096
#define MAX_URL_LENGTH 2048
#define DEFAULT_HTTP_PORT "80"
#define SOCKET_TIMEOUT 3  // 3 seconds timeout

typedef struct {
    char *method;
    char *url;
    char *host;
    char *port;
    char *path;
    char *data;
    char *headers[MAX_HEADERS];
    int header_count;
    int show_headers;
} HttpRequest;

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s [<OPTIONS>] <URL>\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h          Show this help message\n");
    fprintf(stderr, "  -H 'Header: Content'  Add custom request header (can be used multiple times)\n");
    fprintf(stderr, "  -X 'METHOD' Set request method (GET/POST/PUT/DELETE)\n");
    fprintf(stderr, "  -d 'data'   Set request body\n");
    fprintf(stderr, "  -i          Show response headers in output\n");
}

// Set socket timeout
int set_socket_timeout(int sockfd, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        return -1;
    }
    
    return 0;
}

// Parse URL to extract host, port, and path
int parse_url(const char *url, char **host, char **port, char **path) {
    if (strncmp(url, "http://", 7) != 0) {
        fprintf(stderr, "Error: Only HTTP protocol is supported\n");
        return -1;
    }
    
    const char *host_start = url + 7; // Skip "http://"
    const char *port_start = NULL;
    const char *path_start = NULL;
    const char *fragment_start = NULL;
    
    // Find fragment start (and ignore it)
    fragment_start = strchr(host_start, '#');
    
    // Find path start
    path_start = strchr(host_start, '/');
    if (path_start == NULL || (fragment_start != NULL && fragment_start < path_start)) {
        // No path specified, use "/"
        *path = strdup("/");
    } else {
        // Copy path up to fragment if exists
        if (fragment_start != NULL) {
            size_t path_len = fragment_start - path_start;
            *path = malloc(path_len + 1);
            strncpy(*path, path_start, path_len);
            (*path)[path_len] = '\0';
        } else {
            *path = strdup(path_start);
        }
    }
    
    // Find port if specified
    port_start = strchr(host_start, ':');
    if (port_start != NULL && (path_start == NULL || port_start < path_start) 
                           && (fragment_start == NULL || port_start < fragment_start)) {
        // Port specified
        size_t host_len = port_start - host_start;
        *host = malloc(host_len + 1);
        strncpy(*host, host_start, host_len);
        (*host)[host_len] = '\0';
        
        // Extract port
        const char *end_ptr = (path_start != NULL) ? path_start : 
                             ((fragment_start != NULL) ? fragment_start : host_start + strlen(host_start));
        size_t port_len = end_ptr - port_start - 1;
        *port = malloc(port_len + 1);
        strncpy(*port, port_start + 1, port_len);
        (*port)[port_len] = '\0';
    } else {
        // No port specified, use default port 80
        size_t host_end = (path_start != NULL) ? path_start - host_start : 
                         ((fragment_start != NULL) ? fragment_start - host_start : strlen(host_start));
        *host = malloc(host_end + 1);
        strncpy(*host, host_start, host_end);
        (*host)[host_end] = '\0';
        *port = strdup(DEFAULT_HTTP_PORT);
    }
    
    return 0;
}

// Connect to the server
int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd, status;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }
    
    // Try to connect to an address
    for (p = res; p != NULL; p = p->ai_next) {
        // Create socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            continue; // Try next address
        }
        
        // Connect
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue; // Try next address
        }
        
        break; // Connection successful
    }
    
    if (p == NULL) {
        fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    
    // Set socket timeout
    if (set_socket_timeout(sockfd, SOCKET_TIMEOUT) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Send HTTP request
int send_request(int sockfd, HttpRequest *req) {
    char request[BUFFER_SIZE];
    int request_len = 0;
    
    // Add request line
    request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                           "%s %s HTTP/1.1\r\n", req->method, req->path);
    
    // Add Host header if not already present
    int host_header_found = 0;
    for (int i = 0; i < req->header_count; i++) {
        if (strncasecmp(req->headers[i], "Host:", 5) == 0) {
            host_header_found = 1;
            break;
        }
    }
    
    if (!host_header_found) {
        // Include port in Host header if not default (80)
        if (strcmp(req->port, "80") == 0) {
            request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                                   "Host: %s\r\n", req->host);
        } else {
            request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                                   "Host: %s:%s\r\n", req->host, req->port);
        }
    }
    
    // Add Connection: close header to ensure server closes connection
    request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                           "Connection: close\r\n");
    
    // Add Content-Length header if there's data
    if (req->data != NULL) {
        int content_length_found = 0;
        for (int i = 0; i < req->header_count; i++) {
            if (strncasecmp(req->headers[i], "Content-Length:", 15) == 0) {
                content_length_found = 1;
                break;
            }
        }
        
        if (!content_length_found) {
            request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                                   "Content-Length: %zu\r\n", strlen(req->data));
        }
    }
    
    // Add custom headers
    for (int i = 0; i < req->header_count; i++) {
        request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, 
                               "%s\r\n", req->headers[i]);
    }
    
    // Add empty line to mark end of headers
    request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, "\r\n");
    
    // Add data if present
    if (req->data != NULL) {
        request_len += snprintf(request + request_len, BUFFER_SIZE - request_len, "%s", req->data);
    }
    
    // Send the request
    ssize_t bytes_sent = send(sockfd, request, request_len, 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Failed to send request\n");
        return -1;
    }
    
    return 0;
}

// Check if response is chunked
int is_chunked_response(const char *headers) {
    const char *transfer_encoding = strcasestr(headers, "Transfer-Encoding:");
    if (transfer_encoding && strcasestr(transfer_encoding, "chunked")) {
        return 1;
    }
    return 0;
}

// Process HTTP response
int process_response(int sockfd, int show_headers) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int headers_end_found = 0;
    char *headers = NULL;
    size_t headers_size = 0;
    size_t headers_capacity = 0;
    int chunked = 0;
    
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        
        if (!headers_end_found) {
            // Look for the end of headers
            char *body_start = strstr(buffer, "\r\n\r\n");
            
            if (body_start) {
                headers_end_found = 1;
                
                // Calculate header and body sizes
                size_t header_part_len = body_start - buffer + 4; // Include the \r\n\r\n
                size_t body_part_len = bytes_received - header_part_len;
                
                // Save headers if needed
                if (show_headers || 1) { // Always save headers to check for chunked encoding
                    // Resize headers buffer if needed
                    size_t new_size = headers_size + header_part_len;
                    if (new_size > headers_capacity) {
                        headers_capacity = new_size + BUFFER_SIZE;
                        headers = realloc(headers, headers_capacity);
                        if (!headers) {
                            fprintf(stderr, "Memory allocation error\n");
                            return -1;
                        }
                    }
                    
                    // Copy headers
                    memcpy(headers + headers_size, buffer, header_part_len - 2); // Exclude the empty line
                    headers_size += header_part_len - 2;
                    headers[headers_size] = '\0';
                    
                    // Check if response is chunked
                    chunked = is_chunked_response(headers);
                    
                    // Print headers if requested
                    if (show_headers) {
                        printf("%s\n", headers);
                    }
                }
                
                // Print body part if any
                if (body_part_len > 0) {
                    fwrite(body_start + 4, 1, body_part_len, stdout);
                }
            } else {
                // Still in headers
                if (show_headers || 1) { // Always save headers to check for chunked encoding
                    // Resize headers buffer
                    size_t new_size = headers_size + bytes_received;
                    if (new_size > headers_capacity) {
                        headers_capacity = new_size + BUFFER_SIZE;
                        headers = realloc(headers, headers_capacity);
                        if (!headers) {
                            fprintf(stderr, "Memory allocation error\n");
                            return -1;
                        }
                    }
                    
                    // Append to headers
                    memcpy(headers + headers_size, buffer, bytes_received);
                    headers_size += bytes_received;
                    headers[headers_size] = '\0';
                }
            }
        } else {
            // Already in body, just print
            fwrite(buffer, 1, bytes_received, stdout);
        }
    }
    
    if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "Error receiving response: %s\n", strerror(errno));
        free(headers);
        return -1;
    }
    
    free(headers);
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    HttpRequest req = {0};
    
    // Set default method
    req.method = "GET";
    
    // Parse command-line options
    while ((opt = getopt(argc, argv, "hH:X:d:i")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return 0;
            case 'H':
                if (req.header_count < MAX_HEADERS) {
                    req.headers[req.header_count++] = optarg;
                } else {
                    fprintf(stderr, "Too many headers\n");
                    return 1;
                }
                break;
            case 'X':
                req.method = optarg;
                break;
            case 'd':
                req.data = optarg;
                // Default to POST if method not specified and data is provided
                if (strcmp(req.method, "GET") == 0) {
                    req.method = "POST";
                }
                break;
            case 'i':
                req.show_headers = 1;
                break;
            default:
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                usage(argv[0]);
                return 1;
        }
    }
    
    // Check if URL is provided
    if (optind >= argc) {
        fprintf(stderr, "Error: URL is required\n");
        usage(argv[0]);
        return 1;
    }
    
    // Get URL
    req.url = argv[optind];
    
    // Parse URL
    if (parse_url(req.url, &req.host, &req.port, &req.path) != 0) {
        return 1;
    }
    
    // Connect to server
    int sockfd = connect_to_server(req.host, req.port);
    if (sockfd < 0) {
        free(req.host);
        free(req.port);
        free(req.path);
        return 1;
    }
    
    // Send request
    if (send_request(sockfd, &req) != 0) {
        close(sockfd);
        free(req.host);
        free(req.port);
        free(req.path);
        return 1;
    }
    
    // Process response
    if (process_response(sockfd, req.show_headers) != 0) {
        close(sockfd);
        free(req.host);
        free(req.port);
        free(req.path);
        return 1;
    }
    
    // Clean up
    close(sockfd);
    free(req.host);
    free(req.port);
    free(req.path);
    
    return 0;
}