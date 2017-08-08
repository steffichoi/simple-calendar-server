#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <time.h>
#include "lists.h"

#ifndef PORT
#define PORT 51969
#endif

#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 8
#define DELIM " \n"
#define MAX_SIZE 256

static int listenfd;

//initialise the heads of the two empty data-structures
Calendar *calendar_list = NULL;
User *user_list = NULL;


//modified client struct from muffinman.c
struct client {
    int fd;
    char * username;  //clients username
    char * buf;  //individual buffer
    struct in_addr ipaddr;
    struct client *next;
} *top = NULL;

static void addclient(int fd, struct in_addr addr, char * username, char * buf);
static void removeclient(int fd);


//taken from muffinman.c
/*
 * Print a formatted error message to stderr.
 */
void error(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}


//taken form calendar.c
/*
 * Return a calendar time representation of the time specified
 *  in local_args.
 *    local_argv[0] must contain hour.
 *    local_argv[1] may contain day, otherwise use current day
 *    local_argv[2] may contain month, otherwise use current month
 *    local_argv[3] may contain day, otherwise use current year
 */
time_t convert_time(int local_argc, char** local_argv) {
    
    time_t rawtime;
    struct tm * info;
    
    // Instead of expicitly setting the time, use the current time and then
    // change some parts of it.
    time(&rawtime);            // get the time_t represention of the current time
    info = localtime(&rawtime);
    
    // The user must set the hour.
    info->tm_hour = atof(local_argv[0]);
    
    // We don't want the current minute or second.
    info->tm_min = 0;
    info->tm_sec = 0;
    
    if (local_argc > 1) {
        info->tm_mday = atof(local_argv[1]);
    }
    if (local_argc > 2) {
        // months are represented counting from 0 but most people count from 1
        info->tm_mon = atof(local_argv[2])-1;
    }
    if (local_argc > 3) {
        // tm_year is the number of years after the epoch but users enter the year AD
        info->tm_year = atof(local_argv[3])-1900;
    }
    
    // start off by assuming we won't be in daylight savings time
    info->tm_isdst = 0;
    mktime(info);
    // need to correct if we guessed daylight savings time incorrect since
    // mktime changes info as well as returning a value. Check it out in gdb.
    if (info->tm_isdst == 1) {
        // this means we guessed wrong about daylight savings time so correct the hour
        info->tm_hour--;
    }
    return  mktime(info);
}


//taken from calendar.c; modified to write results to a client's socket
/*
 * Read and process calendar commands
 * Return:  -1 for quit command
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, Calendar **calendar_list_ptr,
                 User **user_list_ptr, int fd) {
    
    Calendar *calendar_list = *calendar_list_ptr;
    User *user_list = *user_list_ptr;
    char * buf;
    
    if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return -1;
        
    }
    
    else if (strcmp(cmd_argv[0], "add_calendar") == 0 && cmd_argc == 2) {
        
        if (add_calendar(&calendar_list, cmd_argv[1]) == -1) {
            buf = "Calendar by this name already exists\n";
        }
        
        //testing
        printf("added %s\n", cmd_argv[1]);
        //adds to the right calendar but doesnt seem to be modifying it??
        if (calendar_list) {
            buf = list_calendars(calendar_list);
            printf("in list %s\n", buf);
        }
        
    }
    
    else if (strcmp(cmd_argv[0], "list_calendars") == 0 && cmd_argc == 1) {
        //set buf to the value returned by the list_calendars function
        buf = list_calendars(calendar_list);
        printf("buf %s\n", buf);
        write(fd, buf, strlen(buf));
    }
    
    else if (strcmp(cmd_argv[0], "add_event") == 0 && cmd_argc >= 4) {
        // Parameters for convert_time are the values in the array
        // cmd_argv but starting at cmd_argv[3]. The first 3 are not
        // part of the time.
        // So subtract these 3 from the count and pass the pointer
        // to element 3 (where the first is element 0).
        time_t time = convert_time(cmd_argc-3, &cmd_argv[3]);
        
        if (add_event(calendar_list, cmd_argv[1], cmd_argv[2], time) == -1) {
            buf = "Calendar by this name does not exist\n";
            write(fd, buf, strlen(buf));
        }
        
    }
    
    else if (strcmp(cmd_argv[0], "list_events") == 0 && cmd_argc == 2) {
        //set buf to the value returned by the list_events function
        buf = list_events(calendar_list, cmd_argv[1]);
        
        if (strcmp(buf, "-1") == 0) {
            buf = "Calendar by this name does not exist\n";
            write(fd, buf, strlen(buf));
        }
        
        else {
            printf("buf %s\n", buf);
            write(fd, buf, strlen(buf));
        }
        
    }
    
    else if (strcmp(cmd_argv[0], "add_user") == 0 && cmd_argc == 2) {
        if (add_user(user_list_ptr, cmd_argv[1]) == -1) {
            buf = "User by this name already exists\n";
            write(fd, buf, strlen(buf));
        }
        
    }
    
    else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
        //set buf to the value returned by the list_users function
        buf = list_users(user_list);
        printf("buf %s\n", buf);
        write(fd, buf, strlen(buf));
    }
    
    else if (strcmp(cmd_argv[0], "subscribe") == 0 && cmd_argc == 3) {
        int return_code = subscribe(user_list, calendar_list, cmd_argv[1],
                                    cmd_argv[2]);
        if (return_code == -1) {
            buf = "User by this name does not exist\n";
            write(fd, buf, strlen(buf));
        }
        
        else if (return_code == -2) {
            buf = "Calendar by this name does not exist\n";
            write(fd, buf, strlen(buf));
        }
        
        else if (return_code == -3) {
            buf = "This user is already subscribed to this calendar\n";
            write(fd, buf, strlen(buf));
        }
        
    }
    
    else {
        buf = "Incorrect syntax\n";
        write(fd, buf, strlen(buf));
    }
    return 0;
}


//my answer for lab8; client does not seem to be sending '\r',
//so htis function is modified to checks for '\n'
int find_network_newline(char *buf, int inbuf) {
    
    int i=0;
    //    int j=1;
    
    while (i < inbuf) {
        if (buf[i] == '\n') { //&& buf[j] == '\n'){
            return i;
        }
        i++; //j++;
    }
    return -1; // return the location of '\r' if found; in this canse '\n'
}


//parts of this main is taken from muffinman.c and calendar.c, will be specified
int main(int argc, char **argv) {
    struct client *p; //create a client
    
    void bindandlisten(void), newconnection(void);
    char * getcommands(struct client *p);
    void tokenizeandprocess(struct client *p, char cal_command[]);
    
    //taken from muffinman.c; added setsockopt()
    bindandlisten();  /* aborts on error */
    
    //client and select setup taken from muffinman.c
    /* the only way the server exits is by being killed */
    while (1) {
        fd_set fdlist;
        int maxfd = listenfd;
        FD_ZERO(&fdlist);
        FD_SET(listenfd, &fdlist);
        
        //taken from muffinman.c
        for (p = top; p; p = p->next) {
            FD_SET(p->fd, &fdlist);
            if (p->fd > maxfd)
                maxfd = p->fd;
        }
        
        //taken from muffinman.c
        if (select(maxfd + 1, &fdlist, NULL, NULL, NULL) < 0) {
            perror("select");
        }
        
        //if select returns an int >= 0, a client is ready to send the server
        //something
        else {
            
            //taken from muffinman.c
            //go though each client in the client list to check if they
            //are in the FD_ISSET list, meaning they are ready to send
            //something to the server
            for (p = top; p; p = p->next)
                if (FD_ISSET(p->fd, &fdlist))
                    break;
            
            //if we found a client that wants to say something
            //we want to process their command
            if (p) {
                char cal_command[INPUT_BUFFER_SIZE];
                char * commands;
                
                commands = getcommands(p);
                
                //put the commands processes in getcommands into cal_command
                sprintf(cal_command, commands);
                
                //tokenize the commands received from the client
                //and process the command
                tokenizeandprocess(p, cal_command);
                
            }
            
            //taken from muffinman.c;
            //we found a client that is not part of the client list
            //this means a new connection with a new client has been made
            //we want to add this client to the client list
            if (FD_ISSET(listenfd, &fdlist)) {
                //modified to not blobk if  multiple clients connect
                //at the same time
                newconnection();
            }
            
            //add calendar is being buggy... so i hard coded this
            add_calendar(&calendar_list, "something");
            
        }
    }
    return(0);
}


//taken from muffinman.c;
//added setsockopt so the port can be reused immediately after the
//server is killed
void bindandlisten(void) { /* bind and listen, abort on error */
    struct sockaddr_in r;
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    
    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);
    
    int on = 1;
    int status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if(status == -1) {
        perror("setsockopt -- REUSEADDR");
    }
    
    
    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }
    
    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}


//parts of this taken from muffinman.c
void newconnection(void) {
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;
    int i, c;
    
    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0) {
        perror("accept");
    }
    
    else {
        char buf[INPUT_BUFFER_SIZE];
        printf("connection from %s\n", inet_ntoa(r.sin_addr));
        
        //a client has connected, ask it for its username
        sprintf(buf, "What is your user name?\r\n");
        write(fd, buf, strlen(buf));
        
        c = -2;  /* (neither a valid char nor the -1 error signal below) */
        while (c == -2) {
            fd_set fdlist;
            FD_ZERO(&fdlist);
            FD_SET(fd, &fdlist);
            
            if (select(fd+1, &fdlist, NULL, NULL, NULL) != 1) {
                c = -1;
                break;
            }
            
            int inbuf=0; // how many bytes currently in buffer?
            int room=sizeof(buf); // how much room left in buffer?
            char *after; // pointer to position after the data in buf
            int where; // location of network newline
            
            int nbytes = read(fd, buf, sizeof buf);
            if (nbytes > 0) {
                c = -1;
                
                printf("New connection on port %d\n", ntohs(r.sin_port));
                
                after = buf;        // start writing at beginning of buf
                inbuf = nbytes+(sizeof(buf)-room);
                where = find_network_newline(buf, inbuf);
                
                if (where > 0) {
                    buf[where] = '\0';
                    
                    memmove(buf, &buf[where+2], (inbuf-(where+1)));
                    
                    inbuf = inbuf-where-2;
                    where = -1;
                }
                room = sizeof(buf)-inbuf;
                after = buf+inbuf;
                
                addclient(fd, r.sin_addr, buf, buf);
                
                sprintf(buf, "Go ahead and enter calendar commands>\n");
                write(fd, buf, strlen(buf));
                
                for (i = 0; i < nbytes; i++) {
                    if (isascii(buf[i]) && !isspace(buf[i])) {
                        c = buf[i];
                        break;
                    }
                }
            }
            
            else { //if (nbytes < 0)
                if (errno != EINTR)
                    c = -1;
            }
        }
    }
}


//get the command from the clients socket and return it as a char *
char * getcommands(struct client *p) {
    
    char commands[80];
    
    int inbuf=0; // how many bytes currently in buffer?
    int room=sizeof(commands); // how much room left in buffer?
    char *after; // pointer to position after the data in buf
    int where; // location of network newline
    
    int nbytes = read(p->fd, commands, sizeof commands);
    if (nbytes > 0) {
        
        after = commands;        // start writing at beginning of buf
        inbuf = nbytes+(sizeof(commands)-room);
        where = find_network_newline(commands, inbuf);
        
        if (where > 0) {
            commands[where] = '\0';
            
            memmove(commands, &commands[where+2], (inbuf-(where+1)));
            
            inbuf = inbuf-where-2;
            where = -1;
            
        }
        room = sizeof(commands)-inbuf;
        after = commands+inbuf;
        printf("Server got %s\n", commands);
        return commands;
    }
    
    else {
        /* shouldn't happen */
        removeclient(p->fd);
        return "0";
    }
}


void tokenizeandprocess(struct client *p, char cal_command[]) {
    
    //break up cal_command, tokenize it..., put them into command_list
    
    char *next_token = strtok(cal_command, DELIM);
    int cmd_argc = 0;
    char *cmd_argv[INPUT_ARG_MAX_NUM];
    
    printf("next token %s\n", next_token);
    
    while (next_token != NULL) {
        if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
            error("Too many arguments!");
            cmd_argc = 0;
            break;
        }
        cmd_argv[cmd_argc] = next_token;
        cmd_argc++;
        next_token = strtok(NULL, DELIM);
    }

    printf("processing %s %s\n", cmd_argv[0], cmd_argv[1]);
    
    if ((process_args(cmd_argc, cmd_argv, &calendar_list, &user_list,
                      p->fd)) == -1) {
        char buf[80];
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        fflush(stdout);
        sprintf(buf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        removeclient(p->fd);
        write(p->fd, buf, strlen(buf));
    }
}


//taken from muffinman.c;
//modified to add username and buffer
static void addclient(int fd, struct in_addr addr, char * username, char * buf) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        fprintf(stderr, "out of memory!\n");  /* highly unlikely to happen */
        exit(1);
    }
    printf("Adding client %s\n", inet_ntoa(addr));
    fflush(stdout);
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    
    p->username = malloc(INPUT_BUFFER_SIZE);
    strcpy(p->username, username);
    
    p->buf = malloc(INPUT_BUFFER_SIZE);
    sprintf(p->buf, buf);
    
    top = p;
}


//taken from muffinman;
//modified to free malloc-ed username and buf pointers
static void removeclient(int fd) {
    struct client **p;
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %s\n", inet_ntoa((*p)->ipaddr));
        fflush(stdout);
        
        //these error at compile time. dont know why yet! NEED TO FIX
        //        free(p->username);
        //        free(p->buf);
        
        free(*p);
        *p = t;    }
    
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
}