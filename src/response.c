#include "response.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
 
#define MAX_HEADERS 32
#define HEADER_NAME_MAX 128
#define HEADER_VALUE_MAX 512
 
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
 
/* ---------- Response helper ---------- */
 
static void SendResponse(int fd, int status, const char *status_text,
                         const char *content_type, const char *body) {
    char response[4096];
    int body_len = body ? (int)strlen(body) : 0;
 
    if (body && body_len > 0) {
        snprintf(response, sizeof(response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            status, status_text, content_type, body_len, body);
    } else {
        snprintf(response, sizeof(response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            status, status_text);
    }
 
    if (send(fd, response, strlen(response), 0) == -1)
        perror("send failed");
}
 
/* ---------- Request handler ---------- */
 
void SendHTTPResponse(int client_fd) {
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
 
    /* --- 404 fallback --- */
    else {
        SendResponse(client_fd, 404, "Not Found", NULL, NULL);
    }
}
 