#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>         	          /* host to IP resolution            */
#include <sys/stat.h>
#include<arpa/inet.h>

#define  END_OF_HEADER "\r\n\r\n"
#define REQUEST_SIZE 1000
#define RESPONSE_SIZE 1000
#define BUFLEN      100
#define ERROR400 "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>"
#define ERROR404 "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>"
#define ERROR403 "HTTP/1.0 403 Forbidden\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>"
#define ERROR500 "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>"
#define ERROR501 "HTTP/1.0 501 Not supported\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.\r\n</BODY></HTML>"


//extern int errno;
int array_length;

void error(char* str){
    perror(str);
    exit(EXIT_FAILURE) ;
}

void free_array(char ** array){
    int i = 0;
    while(i < array_length)
    {
        free(array[i]);
        i++;
    }
    free(array);
}

void free_all(char** ip, char **path, char * buf, DIR* dir)
// free the given pointers
{
    if(*ip != NULL)
    free(*ip);
    if(*path != NULL)
    free(*path);
    if(buf != NULL)
    free(buf);
    if(dir != NULL)
        closedir(dir);
}

void process_request(char request[], int *response_length, int sockfd);
void read_request(int fd, char buf[]);
//void  send_response(int fd, char *response, int response_length);
//int correct_read(int s, char *data, int len);
int  hendle_client(void* st);
//int correct_write(int s, char *data, int len);
void read_file_to_array(char * fname);
void send_to_server(char * host, char path[], int client_socket, char request[]);
char *get_mime_type(char *name);
unsigned long ip_to_int(char ip[]);
int is_in_filter(char * host, int sockfd);


int count_occurances(char * str, char toSearch)
//count occurances of 'toSearch' in 'str'
{
    int count = 0, i = 0;
    while(str[i] != '\0')
    {
        /*
         * If character is found in string then
         * increment count variable
         */
        if(str[i] == toSearch)
        {
            count++;
        }

        i++;
    }
    return  count;
}
static char **array;
int main(int argc, char * argv[]) {
    threadpool * tp;
    int main_socket, serving_socket, rc, amount_threads, request_amount, i;
    struct sockaddr_in serv_addr;
    if(argc != 5 || atoi(argv[1]) < 1024 || atoi(argv[1]) > 10000 || atoi(argv[2]) < 0 || atoi(argv[3]) < 0) {                 //port?
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        return 0;
    }
    amount_threads = atoi(argv[2]);
    request_amount = atoi(argv[3]);

    main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket < 0) {
        error("error: socket\n");
    }


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr =INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));


    rc = bind(main_socket, (struct sockaddr *) &serv_addr,  sizeof(serv_addr));
    if (rc < 0) {
        close(main_socket);
        error("error: bind\n");
    }

    rc = listen(main_socket, 5);
    if (rc < 0)
        error("error: listen\n");

    tp = create_threadpool(amount_threads);
    if(tp == NULL){
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        exit(EXIT_FAILURE);
    }

    read_file_to_array(argv[4]);

    for( i = 0; i < request_amount; i++) {
        serving_socket = accept(main_socket, NULL, NULL);
        if (serving_socket < 0) {
            error("error: accept\n");
            free_array(array);
        }
        dispatch(tp, hendle_client, &serving_socket);
    }
    close(main_socket);
    destroy_threadpool(tp);
    free_array(array);
    return 0;
}

void send_to_server(char * host, char * path, int client_socket, char * request){
    char c, *temp = NULL;
    FILE * fp;
    int port = 80, sz, count_readen_wordes = 15, index;
    int num, i = 0, ch = 0, flag = 0, sockfd = -1, rc, rec;
    char rbuf[BUFLEN], *rbuf1 = NULL, *str = NULL;
    DIR * dir = NULL;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buf3[RESPONSE_SIZE+17];

    /*check for edge cases and fix if need*/
    if(strcmp(path, "/") == 0)
    {
        //free(path);
        //path = NULL;
        path = (char*) malloc(12*sizeof(char));
        //strcpy(path, "/index.html");
        sprintf(path,"%s","/index.html");
    }
    /*else if(path[strlen(path)-1] == '/')
    {
        path[strlen(path)-1] = '\0';
    }*/
    str = (char*)malloc(strlen(host)+ strlen(path)+1);
    sprintf(str, "%s%s", host, path); //make an URL without http://
    if(access(str, F_OK) == 0 && ((dir =opendir(str)) == NULL)) // check if the direction already exists
    {
        fp = fopen(str, "r");
        if(fp < 0){
            free_all(&host, &path, rbuf1, dir);
            free(str);
            write(client_socket, ERROR500, strlen(ERROR500));
            close(client_socket);
        }
        fseek(fp, 0L, SEEK_END); // check file length
        sz = (int)ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        if(get_mime_type(str) != NULL)
            sprintf(buf3, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nContent-type: %s\r\nconnection: Close\r\n\r\n", sz, get_mime_type(str) );
        else
            sprintf(buf3, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nconnection: Close\r\n\r\n", sz );
        write(client_socket, buf3, strlen(buf3));
        for (i = 0; i < sz; i++) {//read from the file to the socket
            c=fgetc(fp);
            if(write(client_socket,&c, 1) < 0){
                free_all(&host, &path, NULL, NULL);
                free(str);
                write(client_socket, ERROR500, strlen(ERROR500));
                close(client_socket);
            }
        }
        printf("File is given from local filesystem\n");
        printf("\n Total response bytes: %d\n", (sz+54+(int)sizeof(sz)));
        fclose(fp);
    }
    else
    {
        if(strcmp(path, "/index.html\0") == 0) //no path was excepted
        {
            if((rbuf1 = (char*)malloc(strlen(host)+27)) == NULL)
            {
                write(client_socket, ERROR500, strlen(ERROR500));
                close(sockfd);
                free(str);
            }
            if(strstr(request, "http/1.0") != NULL)
                sprintf(rbuf1,"GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
            else
                sprintf(rbuf1,"GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
            //sprintf(rbuf1,"GET / %s\r\nHost: %s\r\n\r\n",request, host);
            rbuf1[strlen(host) + 26] = '\0';
        }
        else {
            rbuf1 = (char*)malloc(strlen(path)+ strlen(host)+27);
            if(strstr(request, "http/1.0") != NULL)
                sprintf(rbuf1, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
            else
                sprintf(rbuf1, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);
            //sprintf(rbuf1, "GET %s %s\r\nHost: %s\r\n\r\n", path, request, host);
            rbuf1[strlen(path) + strlen(host) + 26] = '\0';
        }
        //printf("%s\n", rbuf1);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            free_all(&host, &path, rbuf1, dir);
            free(str);
            write(client_socket, ERROR500, strlen(ERROR500));
            close(sockfd);
        }

        // connect to server
        server = gethostbyname(host);
        if (server == NULL) {
            free_all(&host, &path, rbuf1, dir);
            write(client_socket, ERROR404, strlen(ERROR404));
            close(sockfd);
            free(str);
        }
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        rc = connect(sockfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if (rc < 0) {
            free_all(&host, &path, rbuf1, dir);
            write(client_socket, ERROR500, strlen(ERROR500));
            close(sockfd);
            free(str);
        }
        //printf("%s\n", request);
        // send and then receive messages from the server
        if(write(sockfd, rbuf1, strlen(rbuf1)) < 0)
        //if(write(sockfd, request, strlen(request)+1) < 0)
        {
            free_all(&host, &path, rbuf1, dir);
            free(str);
            write(client_socket, ERROR500, strlen(ERROR500));
            close(sockfd);
        }
        if((rec = read(sockfd, rbuf,  15)) < 0) //check the first header to see if it is 200 ok respond
        {
            free_all(&host, &path, rbuf1, dir);
            free(str);
            write(client_socket, ERROR500, strlen(ERROR500));
            close(sockfd);
        }
        rbuf[rec] = '\0';
        if(strcmp(rbuf, "HTTP/1.0 200 OK") == 0 || strcmp(rbuf, "HTTP/1.1 200 OK") == 0)
        {
            printf("File is given from origin server\n");
            num = count_occurances(str, '/');
            while ((ch < num || num == 1) && i < strlen(str)) //create direction without the file name
            {
                if(str[i] == '/'){
                    temp = (char *) malloc(i+1);
                    if(temp == NULL)
                    {
                        free_all(&host, &path, rbuf1, dir);
                        write(client_socket, ERROR500, strlen(ERROR500));
                        close(sockfd);
                        free(str);
                    }
                    memcpy(temp, str, i);
                    temp[i] = '\0';
                    ch++;
                    mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); // check
                    free(temp);
                }
                i++;
            }
            fp = fopen(str, "w+"); // open file int the created direction
            if(fp == NULL)
            {
                free_all(&host, &path, rbuf1, dir);
                write(client_socket, ERROR500, strlen(ERROR500));
                close(sockfd);
                free(str);
            }
            if(write(client_socket, rbuf, strlen(rbuf)) < 0)
            {
                //write(sockfd, ERROR500, strlen(ERROR500));
                close(sockfd);
                free(str);
            }
            while ((rc = read(sockfd, rbuf,  BUFLEN-1)) > 0) {
                rbuf[rc] = '\0';
                if (rc > 0) {
                    count_readen_wordes += rc;
                    if(write(client_socket, rbuf, rc) < 0)
                    {
                        //write(sockfd, ERROR500, strlen(ERROR500));
                        close(sockfd);
                        free(str);
                    }
                    if(flag > 3){
                        for(index = 0; index < rc; index++)
                            fputc(rbuf[index], fp);
                    }
                    else{
                        int cun = 0;
                        while(flag <= 3 && rbuf[cun] != '\0')
                        {
                            if(rbuf[cun] == END_OF_HEADER[flag])
                                flag++;
                            else
                                flag = 0;
                            cun++;
                        }
                        if(flag > 3)
                        {
                            while(cun < rc )//rbuf[cun] != '\0')
                            {
                                fputc(rbuf[cun], fp);
                                cun++;
                            }
                        }
                    }
                }
                else {
                    free_all(&host, &path, rbuf1, dir);
                    //write(client_socket, ERROR500, strlen(ERROR500));
                    close(sockfd);
                    free(str);
                    str = NULL;
                }
            }
            fclose(fp);
        }
        else
        {
            //printf("%s", rbuf);
            if(write(client_socket, rbuf, rc) < 0)
            {
                //write(sockfd, ERROR500, strlen(ERROR500));
                close(sockfd);
                free(str);
            }
            while ((rec = read(sockfd, rbuf,  BUFLEN)) > 0) { // print the respond
                if (rec > 0) {
                    rbuf[rec]='\0';
                    count_readen_wordes += rec;
                    //printf("%s", rbuf);
                    if(write(client_socket, rbuf, rc) < 0)
                    {
                        //write(sockfd, ERROR500, strlen(ERROR500));
                        close(sockfd);
                        free(str);
                    }
                }
                else {
                    free_all(&host, &path, rbuf1, dir);
                    //write(sockfd, ERROR500, strlen(ERROR500));
                    close(sockfd);
                    free(str);
                }
            }
        }
        //printf("File is given from origin server\n");
        printf("\n Total response bytes: %d\n", count_readen_wordes);
        close(sockfd);
    }
    if(str != NULL)
        free(str);
    free_all(&host, &path, rbuf1, dir);
}

void process_request(char request[], int *response_length, int sockfd) {
    char *token, *path = NULL, *host = NULL, request_copy[REQUEST_SIZE], req_type[10], pathc[REQUEST_SIZE];
    int flag = 0;
    strcpy(request_copy, request);
    request_copy[strlen(request)] = '\0';
    token = strtok(request, " ");
    if(token == NULL || (strcmp(token, "GET") != 0 && strcmp(token, "POST") != 0 && strcmp(token, "DELETE") != 0
        && strcmp(token, "PUT") != 0 && strcmp(token, "PATCH") != 0 && strcmp(token, "HEAD") != 0
        && strcmp(token, "CONNECT") != 0 && strcmp(token, "TRACE") != 0 && strcmp(token, "OPTIONS") != 0))
    {
        write(sockfd, ERROR400, strlen(ERROR400));
        close(sockfd);
    }
    else {
        //printf("%s\n", request);
        if (strcmp(token, "GET") != 0)
            flag = 1;
        token = strtok(NULL, " ");
        if (token == NULL) {
            write(sockfd, ERROR400, strlen(ERROR400));
            close(sockfd);
            return;
        } else {
            path = (char *) malloc(strlen(token) + 1);
            if (path == NULL) {
                write(sockfd, ERROR500, strlen(ERROR500));
                close(sockfd);
            }
            else {
                strcpy(path, token);
                //strcat(path, "\0");
                path[strlen(token)] = '\0';
                token = strtok(NULL, "\n");
                //if(token == NULL || (strcmp(token, "http/1.0\r") != 0 && strcmp(token, "http/1.1\r") != 0)) {
                if (token == NULL || (strstr(token, "HTTP/1.0") != NULL && strstr(token, "HTTP/1.1") != NULL)) {
                    write(sockfd, ERROR400, strlen(ERROR400));
                    //free(path);
                    close(sockfd);
                } else {
                    strcpy(req_type, token);
                    req_type[strlen(token)] = '\0';
                    token = NULL;
                    token = strtok(NULL, " ");
                    //if(token == NULL || (strcmp(token, "HOST:") != 0)) {
                    if (token == NULL || (strstr(token, "Host:") == NULL)) {
                        //printf("host, token: %s\n", token);
                        write(sockfd, ERROR400, strlen(ERROR400));
                        //free(path);
                        close(sockfd);
                    } else {
                        //token = strtok(NULL, "Host:");
                        token = NULL;
                        token = strtok(NULL, "\n");
                        if (token == NULL) {
                            write(sockfd, ERROR400, strlen(ERROR400));
                            //free(path);
                            close(sockfd);
                        } else {
                            host = (char *) malloc(strlen(token) + 1);
                            if (host == NULL) {
                                write(sockfd, ERROR500, strlen(ERROR500));
                                //free(path);
                                close(sockfd);
                            } else {
                                strcpy(host, token);
                                host[strlen(token) - 1] = '\0';
                                if (flag == 0) {
                                    if (is_in_filter(host, sockfd) == 0) {
                                        if (strstr(path, "http") != NULL) {
                                            token = strtok(path, "//");
                                            token = strtok(NULL, "/");
                                            token = strtok(NULL, "\0");
                                            if (token == NULL) {
//                                pathc[0] = '/';
//                                pathc[1] = '\0';
                                                strcpy(pathc, "/\0");
                                            } else {
                                                strcpy(pathc, "/");
                                                strcat(pathc, token);
                                                pathc[strlen(token) + 1] = '\0';
                                            }

                                        } else {
                                            strcpy(pathc, path);
                                            pathc[strlen(path)] = '\0';
                                        }
                                        printf("HTTP request =\n%s\nLEN = %d\n", request_copy,
                                               (int) strlen(request_copy));
                                        send_to_server(host, pathc, sockfd, req_type);
                                        //send_to_server(host, pathc, sockfd, request_copy);
                                    } else {
                                        write(sockfd, ERROR403, strlen(ERROR403));
                                        //free(path);
                                        close(sockfd);
                                    }
                                } else { //if the request wasnt GET
                                    write(sockfd, ERROR501, strlen(ERROR501));
                                    //free(path);
                                    close(sockfd);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    //if(host != NULL)
        //free(request);
    if(path != NULL)
        free((path));
}

int  hendle_client(void* st){
    char request[REQUEST_SIZE+1];
    //char *response = NULL;
    int * socket_talk_p = (int*) st;
    int socket_talk = *socket_talk_p;
    int ret;

    ret = read(*socket_talk_p, request, REQUEST_SIZE);
    if(ret <= 0){
        write(*socket_talk_p, ERROR500, strlen(ERROR500));
        close(*socket_talk_p);
    }
    request[ret] = '\0';
    //printf("%s\n", request);
        int response_length;

        process_request(request, &response_length, socket_talk);  // step 3
    // clean up allocated memory, if any
//    if (response != NULL)
//        free(response);
    close(socket_talk);  // step 5
    return 0;
}

unsigned long ip_to_int(char ip[]){
    unsigned int c1,c2,c3,c4;
    sscanf(ip, "%d.%d.%d.%d",&c1,&c2,&c3,&c4);
    unsigned long ip1 = c4+c3*256+c2*256*256+c1*256*256*256;
    return ip1;
}

int is_in_filter(char * host, int sockfd){
    int i;
    struct hostent *server;
    char ip[16], *token, mask_number[10], full[25], hostname[25];
    int mask;//create a mask
    struct sockaddr_in serv_addr;
    server = gethostbyname(host);
    if (server == NULL) {
        write(sockfd, ERROR404, strlen(ERROR404));
        close(sockfd);
    }
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    strcpy(hostname, host);
    hostname[strlen(host)] = '\0';
    for(i = 0; i < array_length; i++){
        //printf("%d: %s\n", i, array[i]);
        if(isdigit(array[i][0])){
            strcpy(full, array[i]);
            token = strtok(full, "/");
            if(token != NULL){
                strcpy(ip, token);
                ip[strlen(token)] = '\0';
                //printf("%s\n", ip);
                token = strtok(NULL, "\0");
                if(token != NULL){
                    strcpy(mask_number, token);
                    mask_number[strlen(token)] = '\0';
                    //printf("%s\n", mask_number);
                    mask = (-1)<<(32-atoi(mask_number));
                    //printf("mask: %d\n", mask);
                    if((((int)ip_to_int(ip)) & mask) == (ntohl(serv_addr.sin_addr.s_addr) & mask)) {
                        //printf("yes\n");
                        return 1;
                    }
                }
                //else
                    //printf("error\n");
            }
        }
        else{
            if(strcmp(array[i], hostname) == 0){
                return 1;
            }
        }
    }
    return 0;
}

void read_file_to_array(char * fname){
    // the function read file into array of arrays, each line in array
    FILE *fptr = NULL;
    int i = 0, j = 0;
    char rec;
    //array = NULL;
    //array = (char**)malloc(sizeof(char*));
    //*array = (char*) malloc(sizeof(char));

    fptr = fopen(fname, "r");
    if(fptr < 0)
    {
        printf("fopen\n");
        exit(0);
    }
    while((rec = fgetc(fptr)) != EOF)
    {
        if(j == 0)
        {
            if(i == 0){
                //free(*array);
                //free(array);
                array = (char**)malloc(sizeof(char*));
            }
            else {
                array = (char **) realloc(array, (i + 1) * sizeof(char *));
            }
            array[i] = (char*)malloc(sizeof(char));
        } else{
            //if(rec != '\n')
            array[i] =(char*) realloc((char*)array[i], j+1);
        }
        array[i][j] = rec;
        if(rec == '\n')
        {
            array[i][j] = '\0';
            j = 0;
            i++;
        }
        else{
            j++;
        }
    }
    array_length = i;
    fclose(fptr);
}

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
