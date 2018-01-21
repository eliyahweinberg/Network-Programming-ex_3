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
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
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


#define TIMEBUF 128
#define SUCCESS 0
#define FAILURE -1
#define TRUE 1
#define FALSE 0
#define CONECTION_CLOSED 1
#define NO_SLASH 2
#define NO_PATH 3
#define IS_DIR 4

#define OK 200
#define FOUND 302
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_ERROR 500
#define NOT_SUPPORTED 501

//#define P_DEBUG

typedef int bool_t;

typedef struct _attributes {
    threadpool* pool;
    int curr_req_num;
    int max_requests_num;
    int port;
    int* clients;
    char timebuf[TIMEBUF];
}server_attribs;

typedef struct _headers_attributes {
    int response_headrs_len;
    int status;
    unsigned long content_len;
    char* last_modified;
    char* path;
    char* content_type;
}headers_attribs;

typedef struct _request_attributes {
    char** path_args;
    unsigned char* request;
    int argc;
    int path_lenght;
    int status;
}request_attribs;

//----------------------------------------------------------------------------//
//--------------------------FUNCTION DECLARATION------------------------------//
//----------------------------------------------------------------------------//
int get_time(char* timebuf);

char *get_mime_type(char *name);

server_attribs* init_attribs(int argc, const char * argv[]);

int init_server(int port);

void dealloc_resources(server_attribs* attribs);

void sigpipe_handler(int signum);

int service_client(void* args);

int receive_request(int sock_fd, request_attribs* req_attribs);

char* get_response_content(int status);

char* get_directory_content(char* path);

char* build_resp_head(headers_attribs* resp);

int parse_request(request_attribs* request);

int send_responce(int sock_fd, request_attribs* request);

void dbs_print(char* msg);
//----------------------------------------------------------------------------//
//------------------------------M A I N---------------------------------------//
//----------------------------------------------------------------------------//
int main(int argc, const char * argv[]) {
    int sock_fd;
    int newsock_fd;
    struct sockaddr cli_addr;
    socklen_t clilen = 0;
    /*checking correct usage command*/
    if (argc != 4) {
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
        dbs_print("new connection established");
        if (newsock_fd < 0){
            perror("Error on accept");
            continue;
        }

        attribs->clients[attribs->curr_req_num] = newsock_fd;
        
        dispatch(attribs->pool, service_client,
                 &attribs->clients[attribs->curr_req_num]);
        
        dbs_print("service client done");
        attribs->curr_req_num++;
    }



    dbs_print("all done - shut down");

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

//----------------------------------------------------------------------------//
int service_client(void* args){
    int cli_sock_fd = *(int*)args;
    int status;
    request_attribs req_attribs;
    req_attribs.path_args = NULL;
    req_attribs.request = NULL;
    req_attribs.path_lenght = 0;

    status = receive_request(cli_sock_fd, &req_attribs);
    if (status != CONECTION_CLOSED)
        send_responce(cli_sock_fd, &req_attribs);
    
    close(cli_sock_fd);
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int receive_request(int sock_fd, request_attribs* req_attribs){
    ssize_t rc;
    const int REQUEST_LINE = 4000;
    int offset = 0, request_lenght = 0;
    int r_found = FALSE;
    int n_found = FALSE;
    int i;
    unsigned char* request = (unsigned char*)malloc(REQUEST_LINE*sizeof(char));
    if (!request)
        return FAILURE;
    
    memset(request, '\0', REQUEST_LINE);
    while (TRUE) {
        rc = read(sock_fd, request+offset, REQUEST_LINE-offset);
        if (rc < 0){
            free(request);
            req_attribs->status = INTERNAL_ERROR;
            return FAILURE;
        } else if (rc == 0){
            free(request);
            return CONECTION_CLOSED;
        }
        offset += rc;
        
        
        /*Checking if EOF terminator received
         *loop running only for new data that received*/
        for (i = (offset-(int)rc == 0 ? 0 : offset-(int)rc); i<rc; i++) {
            if (request[i] == '\r')
                r_found = TRUE;
            else if (request[i] == '\n' && i>0 && request[i-1] == '\r')
                n_found = TRUE;
            
            if (r_found && n_found){
                request_lenght = i-1;
                break; //EOF terminator received
            }
        }
        
        /*EOF terminator received or maximal lenght of the line reached*/
        if (request_lenght || offset == REQUEST_LINE)
            break;
    }
    
    if (!request_lenght) {
        free(request);
        req_attribs->status = BAD_REQUEST;
        return FAILURE;
    }
    req_attribs->request = (unsigned char*)
                        malloc(((request_lenght)+1)*sizeof(char));
    if (!req_attribs->request){
        free(request);
        req_attribs->status = INTERNAL_ERROR;
        return FAILURE;
    }
    
    for (i=0; i<request_lenght; i++)
        req_attribs->request[i] = request[i];
    
    req_attribs->request[request_lenght] = '\0';
    req_attribs->status = SUCCESS;
    
 
    free(request);
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int send_responce(int sock_fd, request_attribs* request){
    dbs_print("in send responce");

    char* response_header = NULL;
    char* temp_path = NULL;
    char* index_path = NULL;
    char* index = "index.html";
    unsigned char* content = NULL;
    int i=0;
    int errsv;
    ssize_t offset = 0, wc = 0, total_sent = 0, rc = 0 ;
    struct stat statbuf;
    int flag = SUCCESS;
    bool_t is_dir_content = FALSE, connection_cl = FALSE;
    int data_flag = 0;
    int temp_path_len;
    int file_fd = 0;
    headers_attribs attr;
    attr.content_len = 0;
    attr.content_type = NULL;
    attr.last_modified = NULL;
    attr.path = NULL;
    attr.status = request->status;
    
    if (request->status == INTERNAL_ERROR || request->status == BAD_REQUEST)
        flag = FAILURE;
    else
        flag = parse_request(request);
    
    if (flag == IS_DIR || flag == NO_SLASH){
        temp_path_len = request->path_lenght;
        temp_path = (char*)malloc(sizeof(char)*temp_path_len);

       /*Bulding request path with respect to permissions */
        for (i=0; i < request->argc; i++) {
            if (i==0)
                strcpy(temp_path, request->path_args[i]);
            else
                strcat(temp_path, request->path_args[i]);
            
            data_flag = stat(temp_path, &statbuf);
            if (data_flag == -1) {
                errsv = errno;
                if (errsv == ENOENT || errsv == ENOTDIR)
                    request->status = NOT_FOUND;
                else
                    request->status = INTERNAL_ERROR;
                flag = FAILURE;
                break;
            }
            /*Checking if there is directory in the end of path and '/' missing*/
            else if (i == request->argc-1
                     && flag == NO_SLASH
                     && S_ISDIR(statbuf.st_mode)){
                request->status = FOUND;
                strcat(temp_path, "/");
                attr.path = temp_path;
                flag = FAILURE;
                break;
            }
            /*Checking EXE permissions for directories in the path*/
            else if ((flag == IS_DIR || i != request->argc-1)
                     &&
                     (!(S_IXUSR & statbuf.st_mode) ||
                      !(S_IXGRP & statbuf.st_mode) ||
                      !(S_IXOTH & statbuf.st_mode)) ){
                         request->status = FORBIDDEN;
                         flag = FAILURE;
                         break;
                     }
        }
    }


    if (flag == IS_DIR || flag == NO_PATH) {
        if (flag == IS_DIR){
            temp_path_len = request->path_lenght+(int)strlen(index)+1;
            index_path = (char*)malloc(sizeof(char)*temp_path_len);
            strcpy(index_path, temp_path);
            strcat(index_path, index);
        }
        else if (flag == NO_PATH)
            index_path = strdup(index);
        
        data_flag = stat(index_path, &statbuf);
        if (data_flag == -1) {
            errsv = errno;
            if (errsv != ENOENT){
                request->status = INTERNAL_ERROR;
                flag = FAILURE;
            }
            else if (flag == IS_DIR || flag == NO_PATH ){
                is_dir_content = TRUE;
                request->status = OK;
                free(index_path);
                if (flag == NO_PATH)
                    temp_path = strdup("./");
                if(stat(temp_path, &statbuf) == -1){
                    request->status = INTERNAL_ERROR;
                    flag = FAILURE;
                }
            }
        }
        else {
            free(temp_path);
            temp_path = index_path;
            request->status = OK;
        }
    }
    if (is_dir_content == FALSE && flag != FAILURE){
        if (!(S_IRUSR & statbuf.st_mode) || !(S_IRGRP & statbuf.st_mode)
            || !(S_IROTH & statbuf.st_mode) || !(S_ISREG(statbuf.st_mode))){
            request->status = FORBIDDEN;
            flag = FAILURE;
        }
        else {
            request->status = OK;
            if(get_mime_type(temp_path))
                attr.content_type = strdup(get_mime_type(temp_path));
            file_fd = open(temp_path, O_RDONLY,0);
            if (file_fd == -1){
                request->status = INTERNAL_ERROR;
                flag = FAILURE;
            }
        }
    }
    
    if (flag != FAILURE){
        char timebuf[TIMEBUF];
        strftime(timebuf, TIMEBUF, RFC1123FMT, gmtime(&statbuf.st_mtime));
        attr.last_modified = timebuf;
        if (is_dir_content == TRUE)
            content = (unsigned char*)get_directory_content(temp_path);
    }
    else if (flag == FAILURE){
        content = (unsigned char*)get_response_content(request->status);
    }
    
    if (is_dir_content || flag == FAILURE){
        attr.content_len = strlen((char*)content);
    } else{
        attr.content_len = (unsigned long)statbuf.st_size;
    }
    attr.status = request->status;
    response_header = build_resp_head(&attr);

    signal(SIGPIPE, sigpipe_handler);
    ssize_t headers_size = strlen(response_header);
    while (offset<headers_size) {
        wc = write(sock_fd, response_header+offset, headers_size-offset);
        if (wc == -1){
            connection_cl = TRUE;
            break;
        }
        offset += wc;
    }
    offset = 0;
    if (is_dir_content || flag == FAILURE){
        while (offset<attr.content_len) {
            wc = write(sock_fd, content+offset, attr.content_len-offset);
            if (wc == -1){
                connection_cl = TRUE;
                break;
            }
            offset += wc;
        }
    }
    else {
        
        int filebuff_size = sizeof(char)*1024;
        unsigned char* filebuff = (unsigned char*)malloc(filebuff_size);
        while (total_sent < (unsigned long)statbuf.st_size && !connection_cl) {
            rc = read(file_fd, filebuff, filebuff_size);
            while (offset < rc) {
                wc = write(sock_fd, filebuff+offset, rc-offset);
                if (wc == -1){
                    connection_cl = TRUE;
                    break;
                }
                offset += wc;
            }
            total_sent += offset;
            offset = 0;
        }
        
        
        free(filebuff);
        close(file_fd);
    }
    
    signal(SIGPIPE, SIG_DFL);
    if (request->path_args) {
        for (i=0; i < request->argc; i++)
            free(request->path_args[i]);
        free(request->path_args);
    }
    if (is_dir_content == TRUE)
        free(content);
    if (temp_path)
        free(temp_path);
    if (attr.content_type){
        free(attr.content_type);
    }
    free(response_header);
    free(request->request);
    dbs_print("at send responce finished to send data");
    return SUCCESS;
}

//----------------------------------------------------------------------------//
char* get_response_content(int status){
    if (status == FOUND)
        return "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n<BODY><H4>302 Found</H4>\nDirectories must end with a slash.\n</BODY></HTML>";
    
    else if (status == BAD_REQUEST)
        return "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>";
    
    else if (status == FORBIDDEN)
        return "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>";
    
    else if (status == NOT_FOUND)
        return "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>";
    
    else if (status == INTERNAL_ERROR)
        return "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>";
    
    else if (status == NOT_SUPPORTED)
        return "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>";
    
    return NULL;
}

//----------------------------------------------------------------------------//
char* build_resp_head(headers_attribs* resp){
    int headers_len = 0;
    int i;
    unsigned long temp_c_len = resp->content_len;
    char timebuf [TIMEBUF];
    char* str_cont_len;
    char* phrase = NULL;
    char str_status [4];
    char* headers;
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
    sprintf(str_cont_len, "%lu", resp->content_len);
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
    headers = (char*)malloc(sizeof(char)*(headers_len+1));
    
    strcpy(headers, R_HTTP);
    strcat(headers, str_status);
    strcat(headers, phrase);
    strcat(headers, R_EOL);
    strcat(headers, R_SERVER);
    strcat(headers, R_EOL);
    strcat(headers, R_DATE);
    strcat(headers, timebuf);
    strcat(headers, R_EOL);
    if (resp->status == FOUND) {
        strcat(headers, R_LOC);
        strcat(headers, resp->path);
        strcat(headers, R_EOL);
    }
    if (resp->status != OK){
        strcat(headers, R_DEF_CTYPE);
        strcat(headers, R_EOL);
    } else if (resp->content_type){
        strcat(headers, R_CTYPE);
        strcat(headers, resp->content_type);
        strcat(headers, R_EOL);
    }
    strcat(headers, R_CLEN);
    strcat(headers, str_cont_len);
    strcat(headers, R_EOL);
    if (resp->status == OK){
        strcat(headers, R_LS_MODIFIED);
        strcat(headers, resp->last_modified);
        strcat(headers, R_EOL);
    }
    strcat(headers, R_CONNECTION);
    strcat(headers, R_EOL);
    strcat(headers, R_EOL);
    
    free(str_cont_len);
    free(phrase);
    
    return headers;
}

//----------------------------------------------------------------------------//
int parse_request(request_attribs* request_args){
    char* get = "GET";
    char* get_s = "Get";
    char* http_1_0 = "HTTP/1.0";
    char* http_1_1 = "HTTP/1.1";
    char* request = (char*)request_args->request;
    char* end = request+strlen(request)-strlen(http_1_0);
    int counter = 0, i, stat = IS_DIR;
    int path_len;
    char* str_ptr, *saveptr, *tmp_ptr;
    char* delim = "/";
    
    if ((strncmp(get, request, strlen(get)) != SUCCESS &&
         strncmp(get_s, request, strlen(get_s)) != SUCCESS)
                    || (strcmp(http_1_0, end) != SUCCESS &&
                        strcmp(http_1_1, end) != SUCCESS) ){
        request_args->status = NOT_SUPPORTED;
        return FAILURE;
    }
    
    request += strlen(get);
    *request = '\0';    //cutting GET from the beginning of the request
    request++;
    *(--end) = '\0';    //cutting HTTP from the end of the request
    if (*request == '/' && strlen(request) > 1)
        request++;
    path_len = (int)strlen(request);
    
    for (i=0; i<path_len;i++)
        if (request[i] == '/')
            counter++;
    
    
    if (counter == 1 && strlen(request) == 1){//path contains only '/'
        request_args->argc = 1;
        return NO_PATH;
    } else if (request[path_len-1] != '/'){//path not ends with '/'
        stat = NO_SLASH;
        counter++;
    }
    request_args->argc = counter;
    request_args->path_args = (char**)malloc(sizeof(char*)*counter);
    str_ptr = request;
    for (i=0; i<counter; i++, str_ptr = NULL){
        tmp_ptr = strtok_r(str_ptr, delim, &saveptr);
        if (!tmp_ptr){
            request_args->status = BAD_REQUEST;
            request_args->argc = i;
            return FAILURE;
        }
        request_args->path_args[i] = (char*)
                                    malloc(sizeof(char)*(strlen(tmp_ptr)+2));
        strcpy(request_args->path_args[i], tmp_ptr);
        if (i != counter-1 || stat == IS_DIR)
            strcat(request_args->path_args[i], delim);
           
    }
    request_args->path_lenght = path_len+2;
    
    return stat;
}

//----------------------------------------------------------------------------//
char* get_directory_content(char* path){
    char* dircontent = NULL;
    struct dirent **namelist;
    int lines_number, i, j=0, k;
    const int strings_size = 6;
    bool_t internal_error = FALSE;
    const int ROW_SIZE = 562;
    char *item_path;
    ssize_t size = 0;
    struct stat statbuf;
    char timebuf[TIMEBUF];
    char * strings[] = {"<HTML>\n<HEAD><TITLE>Index of ",
                        "</TITLE></HEAD>\n\n",
                        "<BODY>\n<H4>",
                        "</H4>\n\n",
                        "<table CELLSPACING=8>\n<th>Name</th><th>Last Modified</th><th>Size</th></tr>\n\n",
                        "</table>\n\n<HR>\n\n<ADDRESS>webserver/1.1</ADDRESS>\n\n</BODY></HTML>"
    };
    char* row[] = {"<tr>\n<td><A HREF=\"",
                    "\">",
                    "</A></td><td>",
                    "</td>\n<td>",
                    "</td>\n</tr>\n\n"
    };
    
  
    memset(timebuf, '\0', TIMEBUF);
    lines_number = scandir(path, &namelist, NULL, alphasort);
    if (lines_number < 0 ){
        return NULL;
    }
    size = sizeof(char)*strlen(path)*2;
    for (k=0; k<strings_size; k++)
        size += sizeof(char)*strlen(strings[k]);
    size += sizeof(char)*ROW_SIZE*lines_number;
    
    
    dircontent = (char*)malloc(size);
    if(!dircontent)
        return NULL;
    
    strcpy(dircontent, strings[0]);
    strcat(dircontent, path);
    strcat(dircontent, strings[1]);
    strcat(dircontent, strings[2]);
    strcat(dircontent, path);
    for (k=3; k<strings_size-1; k++)
        strcat(dircontent, strings[k]);
    
    
    for (i=0; i<lines_number; i++) {
        item_path = (char*)malloc(sizeof(char)*
                        ((strlen(path)+1)+(strlen(namelist[i]->d_name)+1)));
        if (!item_path){
            internal_error = TRUE;
            j = i;
            perror("malloc failed");
            break;
        }
        strcpy(item_path, path);
        strcat(item_path, namelist[i]->d_name);
        if (stat(item_path, &statbuf) == -1){
            perror("stat failure");
            puts(item_path);
            internal_error = TRUE;
            j = i;
            free(item_path);
            break;
        }
        strcat(dircontent, row[0]);
        strcat(dircontent,namelist[i]->d_name);
        strcat(dircontent, row[1]);
        strcat(dircontent,namelist[i]->d_name);
        strcat(dircontent, row[2]);
        
        strftime(timebuf, TIMEBUF, RFC1123FMT, gmtime(&statbuf.st_mtime));
        strcat(dircontent, timebuf);
        strcat(dircontent, row[3]);
        if(S_ISREG(statbuf.st_mode)){
            char* file_size = (char*)malloc(sizeof(char)*TIMEBUF);
            sprintf(file_size, "%lu", (unsigned long)statbuf.st_size);
            strcat(dircontent, file_size);
            free(file_size);
        }
        strcat(dircontent, row[4]);
        free(namelist[i]);
        free(item_path);
    }
    
    strcat(dircontent, strings[5]);
    
    if (internal_error) {
        for (; j < lines_number; j++){
            free(namelist[j]);
        }
        free(dircontent);
        free(namelist);
        return NULL;
    }
    
    free(namelist);
    return dircontent;
}

//----------------------------------------------------------------------------//
void dbs_print(char* msg){
#ifdef P_DEBUG
    printf("%s\n",msg);
#endif
}

void sigpipe_handler(int signum){
    fprintf(stderr, "sigpipe received");
}

