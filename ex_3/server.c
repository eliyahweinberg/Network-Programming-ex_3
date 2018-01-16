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
#define R_SERVER "Server: webserver/1.0"
#define R_DATE "Date: "
#define R_LOC "Location: "
#define R_CTYPE "Content-Type: "
#define R_CLEN "Content-Length: "
#define R_LS_MODIFIED "Last-Modified: "
#define R_CONNECTION "Connection: close"

#define TIMEBUF 128
#define SUCCESS 0
#define FAILURE -1
#define TRUE 1
#define FALSE 0

typedef struct _attributes {
    threadpool* pool;
    int curr_req_num;
    int max_requests_num;
    int port;
    int* clients;
    char timebuf[TIMEBUF];
}server_attribs;



//----------------------------------------------------------------------------//
//--------------------------FUNCTION DECLARATION------------------------------//
//----------------------------------------------------------------------------//
int get_time(char* timebuf);

char *get_mime_type(char *name);

server_attribs* init_attribs(int argc, const char * argv[]);

int init_server(int port);

void dealloc_resources(server_attribs* attribs);

int service_client(void* args);

//----------------------------------------------------------------------------//
//------------------------------M A I N---------------------------------------//
//----------------------------------------------------------------------------//
int main(int argc, const char * argv[]) {
    int sock_fd;
    int newsock_fd;
    struct sockaddr cli_addr;
    socklen_t clilen;
    /*checking correct usage command*/
    if (argc != 3) {
        printf(USAGE);
        return FAILURE;
    }
    
    server_attribs* attribs = init_attribs(argc, argv);
    if (!attribs)
        return FAILURE;
    
    sock_fd = init_server(attribs->port);
    if (sock_fd == FAILURE){
        dealloc_resources(attribs);
        exit(EXIT_FAILURE);
    }
    
    while (attribs->curr_req_num < attribs->max_requests_num) {
        newsock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &clilen);
        
        if (newsock_fd < 0){
            perror("Error on accept");
            continue;
        }
        
        attribs->clients[attribs->curr_req_num] = newsock_fd;
        

        attribs->curr_req_num++;
    }
    
    
    close(sock_fd);
    dealloc_resources(attribs);
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

int service_client(void* args){
    
}
