#include "response.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

void SendHTTPResponse(int client_fd){

    const char *response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

    char buffer[4096];
    char method[10], version[20], path[100];
   
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) return;
    
    buffer[bytes_received] = '\0';

    if (sscanf(buffer, "%9s %99s %19s", method, path, version) != 3) {
        return;
    }

    if (strcmp(path, "/") == 0) {
        if (send(client_fd, response, strlen(response), 0) == -1) {
            perror("send failed");
        }
    }
    else {
        response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

        if (send(client_fd, response, strlen(response), 0) == -1) {
            perror("send failed");
        }
    }
}