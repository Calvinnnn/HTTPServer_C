#include "response.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

char* CheckEccoExistsInPath(char *path){
    char* target = "echo";
    char* result = strstr(path,target);
    return result;
}


void SendHTTPResponse(int client_fd){

    char response[4096];

    char buffer[4096];
    char method[10], version[20], path[100];

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        return;
    }

    buffer[bytes_received] = '\0';
    
    if (sscanf(buffer, "%9s %99s %19s", method, path, version) != 3) {
        return;
    }
    char* result = CheckEccoExistsInPath(path);
    if (strcmp(path, "/") == 0) {
        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        if (send(client_fd, response, strlen(response), 0) == -1) {
            perror("send failed");
        }
    }
    else if(result !=NULL ){
        char* target_body = result + strlen("echo") +1;
        
        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            (int)strlen(target_body), 
            target_body
        );
        if(send(client_fd,response,strlen(response),0)==-1){
            perror("send failed");
        }
    }
    else {
        snprintf(response, sizeof(response),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n");

        if (send(client_fd, response, strlen(response), 0) == -1) {
            perror("send failed");
        }
    }
}