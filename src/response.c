#include "response.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>


void SendHTTPResponse(int client_fd){
    const char *response =
    "HTTP/1.1 200 OK\r\n\r\n";
    send(client_fd,response,strlen(response),0);

}