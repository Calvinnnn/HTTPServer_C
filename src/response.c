#include "response.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdbool.h>
#define MAX_HEADERS 32
#define HEADER_NAME_MAX 128
#define HEADER_VALUE_MAX 512
#define MAX_BODY_SIZE (10 * 1024 * 1024) 
 
/* ---------- Data structures ---------- */
 
typedef struct {
    char name[HEADER_NAME_MAX];
    char value[HEADER_VALUE_MAX];
} HttpHeader;
 
typedef struct {
    char method[16];
    char path[256];
    char version[16];
    HttpHeader headers[MAX_HEADERS];
    int header_count;
    char *body;          /* points into raw buffer, NOT heap-allocated */
} HttpRequest;
 
/* ---------- Parser ---------- */
 
/*
 * Parses a raw HTTP request buffer in-place (modifies it with '\0' terminators).
 * Returns 1 on success, 0 on malformed input.
 *
 * HTTP request format:
 *   METHOD /path HTTP/1.x\r\n
 *   Header-Name: Header-Value\r\n
 *   ...
 *   \r\n
 *   [body]
 */
static int ParseHttpRequest(char *buffer, HttpRequest *req) {
    memset(req, 0, sizeof(*req));
 
    /* --- 1. Request line --- */
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) return 0;
    *line_end = '\0';
 
    if (sscanf(buffer, "%15s %255s %15s", req->method, req->path, req->version) != 3)
        return 0;
 
    char *cursor = line_end + 2;  /* skip past \r\n */
 
    /* --- 2. Headers --- */
    while (req->header_count < MAX_HEADERS) {
        line_end = strstr(cursor, "\r\n");
        if (!line_end) break;
        *line_end = '\0';
 
        if (*cursor == '\0') {
            /* blank line = end of headers */
            cursor = line_end + 2;
            break;
        }
 
        char *colon = strchr(cursor, ':');
        if (colon) {
            HttpHeader *h = &req->headers[req->header_count++];
 
            /* name: copy up to the colon */
            size_t name_len = colon - cursor;
            if (name_len >= HEADER_NAME_MAX) name_len = HEADER_NAME_MAX - 1;
            strncpy(h->name, cursor, name_len);
            h->name[name_len] = '\0';
 
            /* value: skip leading spaces */
            char *val = colon + 1;
            while (*val == ' ') val++;
            strncpy(h->value, val, HEADER_VALUE_MAX - 1);
            h->value[HEADER_VALUE_MAX - 1] = '\0';
        }
 
        cursor = line_end + 2;
    }
 
    req->body = cursor;
    return 1;
}
 
/* Case-insensitive header lookup. Returns value string or NULL. */
static const char *GetHeader(const HttpRequest *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0)
            return req->headers[i].value;
    }
    return NULL;
}

static const char *GetBody(const HttpRequest *req){
    return req->body;
}
 
/* ---------- Response helper ---------- */
 
static void SendResponse(int fd, int status, const char *status_text,
                         const char *content_type, const char *body) {
    int body_len = body ? (int)strlen(body) : 0;
 
    /* 256 bytes is more than enough for headers; add body length on top */
    int buf_size = 256 + body_len;
    char *response = malloc(buf_size);
    if (!response) return;
 
    int written;
    if (body && body_len > 0) {
        written = snprintf(response, buf_size,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            status, status_text, content_type, body_len, body);
    } else {
        written = snprintf(response, buf_size,
            "HTTP/1.1 %d %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            status, status_text);
    }
 
    if (written > 0)
        send(fd, response, written, 0);
 
    free(response);
}

/* ---------- FileCheck helper ---------- */


int exists(const char *fname)
{
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}
long int findSize(char file_name[])
{
    // opening the file in read mode
    FILE* fp = fopen(file_name, "r");

    // checking if the file exist or not
    if (fp == NULL) {
        printf("File Not Found!\n");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);

    // calculating the size of the file
    long int res = ftell(fp);

    // closing the file
    fclose(fp);

    return res;
}

 
/* ---------- Request handler ---------- */





 
void SendHTTPResponse(int client_fd,const char* directory) {
    char buffer[4096];
    HttpRequest req;
 
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) return;
    buffer[bytes_received] = '\0';
 
    if (!ParseHttpRequest(buffer, &req)) {
        SendResponse(client_fd, 400, "Bad Request", "text/plain", "Malformed request");
        return;
    }
 
    /* --- Route: GET / --- */
    if(strcasecmp(req.method,"GET")==0){
        if (strcmp(req.path, "/") == 0) {
        SendResponse(client_fd, 200, "OK", NULL, NULL);
        }
 
    /* --- Route: GET /echo/<text> --- */
        else if (strncmp(req.path, "/echo/", 6) == 0) {
            SendResponse(client_fd, 200, "OK", "text/plain", req.path + 6);
        }
    
    /* --- Route: GET /user-agent --- */
        else if (strcmp(req.path, "/user-agent") == 0) {
            const char *ua = GetHeader(&req, "User-Agent");
            if (ua) {
                SendResponse(client_fd, 200, "OK", "text/plain", ua);
            } else {
                SendResponse(client_fd, 400, "Bad Request", "text/plain", "Missing User-Agent header");
            }
        }
              else if (strncmp(req.path, "/files/", 7) == 0) {
            char *file_name = req.path + 7;
 
            if (*file_name == '\0') {
                SendResponse(client_fd, 404, "Not Found", NULL, NULL);
                return;
            }
 
            /*
             * FIX #3: Path traversal guard.
             * Without this, a request like GET /files/../../etc/passwd escapes
             * the directory and lets an attacker read arbitrary files.
             */
            if (strstr(file_name, "..") != NULL || file_name[0] == '/') {
                SendResponse(client_fd, 400, "Bad Request", "text/plain", "Invalid filename");
                return;
            }
 
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, file_name);
 
            /*
             * FIX #5: Eliminated TOCTOU race condition.
             * Old code called exists() -> findSize() -> fopen() as 3 separate
             * filesystem ops. A file could be deleted/replaced between any two.
             * Now we open once and derive the size from the same handle.
             */
            FILE *fp = fopen(full_path, "rb");
            if (!fp) {
                SendResponse(client_fd, 404, "Not Found", NULL, NULL);
                return;
            }
 
            fseek(fp, 0L, SEEK_END);
            long size = ftell(fp);
            rewind(fp);
 
            if (size < 0) {
                fclose(fp);
                SendResponse(client_fd, 500, "Internal Server Error", NULL, NULL);
                return;
            }
 
            char header[256];
            int header_len = snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Content-Length: %ld\r\n"
                "\r\n",
                size);
 
            send(client_fd, header, header_len, 0);
 
            char file_buf[4096];
            size_t bytes_read;
            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) {
                send(client_fd, file_buf, bytes_read, 0);
            }
 
            fclose(fp);
        }
 
        /* --- 404 fallback --- */
        else {
            SendResponse(client_fd, 404, "Not Found", NULL, NULL);
        }
    }
       


    /* --- Route: POST / --- */
        else if (strcasecmp(req.method, "POST") == 0) {
 
        if (strncmp(req.path, "/files/", 7) == 0) {
            char *file_name = req.path + 7;
 
            if (*file_name == '\0') {
                SendResponse(client_fd, 404, "Not Found", NULL, NULL);
                return;
            }
 
            /*
             * seciurty fix
             */
            if (strstr(file_name, "..") != NULL || file_name[0] == '/') {
                SendResponse(client_fd, 400, "Bad Request", "text/plain", "Invalid filename");
                return;
            }
 
            const char *cl_value = GetHeader(&req, "Content-Length");
            if (!cl_value) {
                SendResponse(client_fd, 400, "Bad Request", NULL, NULL);
                return;
            }
 
            /*
             * FIX #1: Replaced VLA `char buff[length+1]` with malloc.
             * VLAs sized from untrusted input can blow the stack or trigger
             * undefined behaviour when length <= 0.
             *
             * FIX #2: Loop recv() until all body bytes are received.
             * A single recv() into a 4096-byte buffer silently drops everything
             * beyond that limit, producing a truncated file with no error.
             */
            int length = atoi(cl_value);
            if (length <= 0 || length > MAX_BODY_SIZE) {
                SendResponse(client_fd, 400, "Bad Request", NULL, NULL);
                return;
            }
 
            char *buff = malloc(length + 1);
            if (!buff) {
                SendResponse(client_fd, 500, "Internal Server Error", NULL, NULL);
                return;
            }
 
            /* Copy whatever body bytes arrived with the headers first */
            int already_have = bytes_received - (int)(req.body - buffer);
            if (already_have < 0) already_have = 0;
            if (already_have > length) already_have = length;
            memcpy(buff, req.body, already_have);
 
            /* Keep reading until we have all `length` bytes */
            int total = already_have;
            while (total < length) {
                int n = recv(client_fd, buff + total, length - total, 0);
                if (n <= 0) break;
                total += n;
            }
            buff[total] = '\0';
 
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, file_name);
 
            FILE *fptr = fopen(full_path, "wb");
            if (!fptr) {
                free(buff);
                SendResponse(client_fd, 500, "Internal Server Error", NULL, NULL);
                return;
            }
 
            fwrite(buff, 1, total, fptr);
            fclose(fptr);
            free(buff);
 
            SendResponse(client_fd, 201, "Created", NULL, NULL);
        }
    }

}
 