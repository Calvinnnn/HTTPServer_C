#include "response.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>


void SendHTTPResponse(int client_fd){

    char *response =
    "HTTP/1.1 200 OK\r\n\r\n";
    char buffer[4096];
    char method[10],version[20],path[100];
   
    
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

    if(bytes_received >0){
        buffer[bytes_received] ='\0';
        sscanf(buffer,"%s %s %s",method,path,version);
        if(strcmp(path,"/")==0){
            send(client_fd,response,strlen(response),0);
        }
        else{
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
            send(client_fd,response,strlen(response),0);
        }
    }


    
}