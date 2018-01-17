//
//  server.c
//  ex_3
//
//  Created by Eliyah Weinberg on 15.1.2018.
//  Copyright © 2018 Eliyah Weinberg. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define USAGE "Usage: server <port> <pool-size> <max-number-of-request>\n"
#define R_EOL "\r\n"
#define R_HTTP "HTTP/1.1 "
#define R_SERVER "Server: webserver/1.1"
#define R_DATE "Date: "
#define R_LOC "Location: "
#define R_CTYPE "Content-Type: "
#define R_DEF_CTYPE "Content-Type: text/html"
#define R_CLEN "Content-Length: "
#define R_LS_MODIFIED "Last-Modified: "
#define R_CONNECTION "Connection: close"
#define R_

#define TIMEBUF 128
#define SUCCESS 0
#define FAILURE -1
#define TRUE 1
#define FALSE 0
#define CONECTION_CLOSED 1

#define OK 200
#define FOUND 302
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_ERROR 500
#define NOT_SUPPORTED 501


typedef struct _attributes {
    threadpool* pool;
    int curr_req_num;
    int max_requests_num;
    int port;
    int* clients;
    char timebuf[TIMEBUF];
}server_attribs;

typedef struct _responce_attributes {
    int response_headrs_len;
    int status;
    int content_len;
    char* last_modified;
    char* path;
    char* content_type;
}response_attribs;

//----------------------------------------------------------------------------//
//--------------------------FUNCTION DECLARATION------------------------------//
//----------------------------------------------------------------------------//
int get_time(char* timebuf);

char *get_mime_type(char *name);

server_attribs* init_attribs(int argc, const char * argv[]);

int init_server(int port);

void dealloc_resources(server_attribs* attribs);

int service_client(void* args);

int receive_request(int sock_fd, unsigned char** buff, int* req_len);

unsigned char* response_500(int* response_len);

unsigned char* response_400(int* response_len);

char* build_resp_head(response_attribs* resp);

int send_responce(int sock_fd, int status, unsigned char* req, int request_len);
//----------------------------------------------------------------------------//
//------------------------------M A I N---------------------------------------//
//----------------------------------------------------------------------------//
int main(int argc, const char * argv[]) {
//    int sock_fd;
//    int newsock_fd;
//    struct sockaddr cli_addr;
//    socklen_t clilen;
//    /*checking correct usage command*/
//    if (argc != 4) {
//        printf(USAGE);
//        return FAILURE;
//    }
//
//    server_attribs* attribs = init_attribs(argc, argv);
//    if (!attribs)
//        return FAILURE;
//
//    sock_fd = init_server(attribs->port);
//    if (sock_fd == FAILURE){
//        dealloc_resources(attribs);
//        exit(EXIT_FAILURE);
//    }
//
//    while (attribs->curr_req_num < attribs->max_requests_num) {
//        newsock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &clilen);
//
//        if (newsock_fd < 0){
//            perror("Error on accept");
//            continue;
//        }
//
//        attribs->clients[attribs->curr_req_num] = newsock_fd;
//
//        service_client(& attribs->clients[attribs->curr_req_num]);
//
//        attribs->curr_req_num++;
//    }
//
//
//    close(sock_fd);
//    dealloc_resources(attribs);
    
   
    return 0;
}

//----------------------------------------------------------------------------//
//------------------------FUNCTIONS IMPLEMENTATION----------------------------//
//----------------------------------------------------------------------------//
int get_time(char* timebuf){
    time_t now;
    now = time(NULL);
    strftime(timebuf, TIMEBUF, RFC1123FMT, gmtime(&now));
    return 0;
}

//----------------------------------------------------------------------------//
char *get_mime_type(char *name){
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0
                            || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0
                            || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0
                            || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

//----------------------------------------------------------------------------//
server_attribs* init_attribs(int argc, const char * argv[]){
    int port = atoi(argv[1]);
    int pool_size = atoi(argv[2]);
    int requests_num = atoi(argv[3]);
    
    if (port < 0 || pool_size < 1 || requests_num < 1 ){
        printf(USAGE);
        return NULL;
    }

    server_attribs* attribs = (server_attribs*)malloc(sizeof(server_attribs));
    if (!attribs){
        perror("attribs malloc failure");
        exit(-1);
    }
    
    attribs->curr_req_num = 0;
    attribs->max_requests_num = requests_num;
    attribs->port = port;
    memset(attribs->timebuf, '\0', TIMEBUF);
    attribs->clients = (int*)calloc(attribs->max_requests_num, sizeof(int));
    if (!attribs->clients){
        free(attribs);
        return NULL;
    }
    attribs->pool = create_threadpool(pool_size);
    if (!attribs->pool){
        free(attribs->clients);
        free(attribs);
        return NULL;
    }
    
    return attribs;
}

//----------------------------------------------------------------------------//
int init_server(int port){
    int sock_fd;
    struct sockaddr_in serv_adr;
    
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0){
        perror("Error on socket openning");
        return FAILURE;
    }
    
    
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY;
    serv_adr.sin_port = htons(port);
    
    if ( bind(sock_fd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) < 0){
        perror("Error on binding");
        return FAILURE;
    }
    
    if (listen(sock_fd, 5) == -1){
        perror("Error on listen");
        return FAILURE;
    }
    return sock_fd;
}

//----------------------------------------------------------------------------//
void dealloc_resources(server_attribs* attribs){
    destroy_threadpool(attribs->pool);
    free(attribs->clients);
    free(attribs);
}

//----------------------------------------------------------------------------//
int service_client(void* args){
    int cli_sock_fd = *(int*)args;
    int status;
    int req_len;
    unsigned char* request;
    status = receive_request(cli_sock_fd, &request, &req_len);
    if (status != CONECTION_CLOSED)
        send_responce(cli_sock_fd, status, request, req_len);

    close(cli_sock_fd);
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int receive_request(int sock_fd, unsigned char** buff, int* req_len){
    ssize_t rc;
    const int REQUEST_LINE = 4000;
    int offset = 0;
    int r_found = FALSE;
    int n_found = FALSE;
    int i;
    *req_len = 0;
    unsigned char* request = (unsigned char*)malloc(REQUEST_LINE*sizeof(char));
    if (!request)
        return FAILURE;
    
    memset(request, '\0', REQUEST_LINE);
    while (TRUE) {
        rc = read(sock_fd, request+offset, REQUEST_LINE-offset);
        if (rc < 0){
            free(request);
            return INTERNAL_ERROR;
        } else if (rc == 0){
            free(request);
            return CONECTION_CLOSED;
        }
        offset += rc;
        /*Checking if EOF terminator received
         *loop running only for new data that received*/
        for (i = (offset == 0 ? offset : offset-(int)rc); i<rc; i++) {
            if (request[i] == '\r')
                r_found = TRUE;
            else if (request[i] == '\n' && i>0 && request[i-1] == '\r')
                n_found = TRUE;
            
            if (r_found && n_found){
                *req_len = i;
                break; //EOF terminator received
            }
        }
        
        /*EOF terminator received or maximal lenght of the line reached*/
        if (*req_len || offset == REQUEST_LINE)
            break;
    }
    
    if (!*req_len) {
        free(request);
        return BAD_REQUEST;
    }
    *buff = (unsigned char*)malloc(((*req_len)+1)*sizeof(char));
    if (!(*buff)){
        free(request);
        return INTERNAL_ERROR;
    }
    
    for (i=0; i<*req_len; i++)
        *buff[i] = request[i];
    
    *buff[*req_len] = '\0';

    free(request);
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int send_responce(int sock_fd, int status, unsigned char* req, int request_len){
    unsigned char* response = NULL;
    int response_len = 0;
    
    if (status == INTERNAL_ERROR)
        response = response_500(&response_len);
    else if (status == BAD_REQUEST)
        response = response_400(&response_len);
    
    
    return SUCCESS;
}

unsigned char* response_500(int* response_len){
    return NULL;
}

unsigned char* response_400(int* response_len){
    return NULL;
}

char* build_resp_head(response_attribs* resp){
    int headers_len = 0;
    int i;
    int temp_c_len = resp->content_len;
    char timebuf [TIMEBUF];
    char* str_cont_len;
    char* phrase = NULL;
    char str_status [4];
    char* response;
    memset(timebuf, '\0', TIMEBUF);
    get_time(timebuf);
    headers_len += strlen(R_HTTP);
    headers_len += 3;//status lenght
    headers_len += (strlen(R_EOL)*6);
    headers_len += strlen(R_SERVER);
    headers_len += strlen(timebuf) + strlen(R_DATE);
    if (resp->status == OK && resp->content_type){
        headers_len += strlen(R_CTYPE);
        headers_len += strlen(R_EOL);
    }
    else if (resp->status != OK){
        headers_len += strlen(R_DEF_CTYPE);
        headers_len += strlen(R_EOL);
    }
    headers_len += strlen(R_CLEN);
    headers_len += strlen(R_CONNECTION);
    
    /*count content lenght digits numner*/
    for (i=0; temp_c_len != 0; i++)
        temp_c_len /= 10;
    
    str_cont_len = (char*)malloc(sizeof(char)*(i+1));
    sprintf(str_cont_len, "%d", resp->content_len);
    sprintf(str_status, "%d", resp->status);
    headers_len += strlen(str_cont_len);
    
    switch (resp->status) {
        case OK:
            phrase = strdup(" OK");
            headers_len += strlen(R_LS_MODIFIED);
            headers_len += strlen(resp->last_modified);
            headers_len += strlen(R_EOL);
            if (resp->content_type)
                headers_len += strlen(resp->content_type);
            break;
            
        case FOUND:
            phrase = strdup(" Found");
            headers_len += strlen(R_LOC);
            headers_len += strlen(R_EOL);
            headers_len += strlen(resp->path);
            break;
            
        case BAD_REQUEST:
            phrase = strdup(" Bad Request");
            break;
            
        case FORBIDDEN:
            phrase = strdup(" Forbidden");
            break;
            
        case NOT_FOUND:
            phrase = strdup(" Not Found");
            break;
            
        case INTERNAL_ERROR:
            phrase = strdup(" Internal Server Error");
            break;
        
        case NOT_SUPPORTED:
            phrase = strdup(" Not supported");
            break;
    }
    headers_len += strlen(phrase);
    
    resp->response_headrs_len = headers_len;
    /*building response*/
    response = (char*)malloc(sizeof(char)*(headers_len+1));
    
    strcpy(response, R_HTTP);
    strcat(response, str_status);
    strcat(response, phrase);
    strcat(response, R_EOL);
    strcat(response, R_SERVER);
    strcat(response, R_EOL);
    strcat(response, R_DATE);
    strcat(response, timebuf);
    strcat(response, R_EOL);
    if (resp->status == FOUND) {
        strcat(response, R_LOC);
        strcat(response, resp->path);
        strcat(response, R_EOL);
    }
    if (resp->status != OK){
        strcat(response, R_DEF_CTYPE);
        strcat(response, R_EOL);
    } else if (resp->content_type){
        strcat(response, R_CTYPE);
        strcat(response, resp->content_type);
        strcat(response, R_EOL);
    }
    strcat(response, R_CLEN);
    strcat(response, str_cont_len);
    strcat(response, R_EOL);
    if (resp->status == OK){
        strcat(response, R_LS_MODIFIED);
        strcat(response, resp->last_modified);
        strcat(response, R_EOL);
    }
    strcat(response, R_CONNECTION);
    strcat(response, R_EOL);
    strcat(response, R_EOL);
    
//    if (resp->content_type)
//        free(resp->content_type);
//    if (resp->last_modified)
//        free(resp->last_modified);
//    if (resp->path)
//        free(resp->path);
    
    free(str_cont_len);
    free(phrase);
    
    return response;
}

