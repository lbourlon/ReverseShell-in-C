#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h> // waitpid

// Size of incomming / outgoing buffer
#define BUFFSIZE 1024

// https://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
// https://www.tutorialspoint.com/unix_sockets/socket_server_example.htm#

static volatile int runVar = 1;
volatile sig_atomic_t runSig = 1;
void intHandler(int sig){
    if(sig == SIGINT){
        runVar = 0;
        runSig = 0;
    }
    printf("Handling my business :)");
}

int atop(char* port){
    int p = atoi(port);
    return (p > 0 && p <= 65536) ? p : 0;
}

void check(int var, char* msg){
    if(var < 0){
        perror(msg);
        exit(1);
    }
}

void print_usage(){
    printf("Usage: main [command]\n");
    printf("Command:\n");
    printf("    help\n");
    printf("    listen  <port>\n");
    printf("    connect <address> <port> \n");
    printf("    proxy   <listen_port> <target_address> <target_port>\n");
}

int clientInit(int port, const char* host_str){
    int error = 0;
    int client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(client_fd , "Could not open socket");

    /* Defining our server parameters */
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    /* Does the dns resolution and copy it to the server*/
    struct hostent *host;
    host = gethostbyname(host_str);
    if(host == NULL) check(-1, "No such host");
    memcpy(&server.sin_addr, host->h_addr_list[0], host->h_length);

    char* address_str = inet_ntoa(server.sin_addr);
    printf("Connecting to %s:%d\n", address_str, port);

    // Try to connect
    error = connect(client_fd, (struct sockaddr *) &server, sizeof(server));
    check(error, "Couldn't connect to server");

    return client_fd; 
}

void client_shell(int client_fd){
    char buf[BUFFSIZE];
    int error;

    pid_t reader = fork();
    check(reader, "Could not fork reader");

    /* This works because the parent and fork share stdin & stdout */
    /* So one handles the read and the other the write & shuting */ 
    int eof_check = 1;

    if(reader == 0){ // Read Fork, handles output 
        signal(SIGINT, intHandler);
        while(runVar) {
            /* Read response */
            memset(buf, 0, BUFFSIZE);
            error = read(client_fd, buf, BUFFSIZE);
            check(error, "Could not read");
            printf("%s", buf);
        }
        shutdown(client_fd, SHUT_RD);
    } else { // Parent node forks handles input

        memset(buf, 0, BUFFSIZE);

        // signal(SIGINT, intHandler);
        // signal(SIGINT, );
        while(fgets(buf, BUFFSIZE, stdin) != NULL){
            error = write(client_fd, buf, strlen(buf));
            check(error, "Could not write");
            memset(buf, 0, BUFFSIZE);
        }
        
        kill(reader, SIGINT);
        shutdown(client_fd, SHUT_WR);

    }
}

void serverInit_shell(int port){
    printf("Starting Server on port : %d\n", port);
    int server_fd, client_fd, error;
    char in_buf[BUFFSIZE];

    struct sockaddr_in server, client;

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check((int)server_fd, "Couldn't open socket");

    // Defining our server sockaddr_in parameters
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // voir si changer après
    server.sin_port = htons(port);

    // Binding the port
    int c_bind = bind(server_fd, (struct sockaddr *)  &server, sizeof(server));
    check(c_bind, "Couldn't bind");

    // Listen for incomming connection
    int backlog = 3; //number of conn
    listen(server_fd, backlog);
    int client_len = sizeof(client);

    struct sigaction act = {
        .sa_handler = intHandler,
        .sa_flags = 0,
    };
    // sigfillset( &act.sa_mask ); 

    // signal(SIGINT, intHandler);
    sigaction( SIGINT, &act, NULL);
    while (runSig || runVar){
        client_fd = accept(server_fd, (struct sockaddr*) &client, &client_len);

        check(client_fd, "Could not accept connection");
        printf("New connection from ");
        printf("%s\n", inet_ntoa(client.sin_addr));

        // Create fork for new connection 
        pid_t pid=fork();
        check(pid, "Could not fork");

        if(pid == 0) { // Handle the connection
            dup2(client_fd, 0); // forked stdin is redirected to client_fd
            dup2(client_fd, 1); // forked stdout is redirected to client_fd
            dup2(1, 2); // forked stderr is redirected to stdout

            // close(client_fd);   // Closes child client socket fd before exec
            close(server_fd);   // 

            char* arguments[5] = {"bash",  "--rcfile", "~/.bashrc", "-s", NULL};
            execvp(arguments[0], arguments);
        } else {
            close(client_fd); // Closes parent child socket
        }
    }
    printf("Is this being executed ? \n");
    shutdown(client_fd, SHUT_RD);
    close(server_fd);
}



void serverInit_oritinal(int port){
    printf("Starting Server on port : %d\n", port);
    int server_fd, client_fd, error;
    char buf[BUFFSIZE];
    struct sockaddr_in server, client;

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check((int)server_fd, "Couldn't open socket");

    // Defining our server sockaddr_in parameters
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // voir si changer après
    server.sin_port = htons(port);

    // Binding the port
    int c_bind = bind(server_fd, (struct sockaddr *)  &server, sizeof(server));
    check(c_bind, "Couldn't bind");

    // Listen for incomming connection
    int backlog = 3; //number of conn
    listen(server_fd, backlog);

    // Accept one connection
    int client_len = sizeof(client);
    client_fd = accept(server_fd, (struct sockaddr*) &client, &client_len);
    check(client_fd, "Could not accept connection");

    signal(SIGINT, intHandler);
    while(runVar){
        // Start communication
        memset(buf, 0, BUFFSIZE);
        error = read(client_fd, buf, BUFFSIZE-1);
        check(error, "Could not read incomming message");
        printf("%s", buf);

        error = write(client_fd, "ACK\n", 4);
        check(error, "Could not respond to client");
    }
    close(client_fd);
    close(server_fd);
}

/* Here's some documentation for this funciton.
 *
 */
void proxy(int listen_port,  const char* host_str, int target_port){
    printf("Listening on %d sending to %s:%d\n", listen_port, host_str, target_port);
    int server_fd, client_fd, error;
    char in_buf[BUFFSIZE];

    struct sockaddr_in server, client;

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check((int)server_fd, "Couldn't open socket");

    // Defining our server sockaddr_in parameters
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // voir si changer après
    server.sin_port = htons(listen_port);

    // Binding the port
    int c_bind = bind(server_fd, (struct sockaddr *)  &server, sizeof(server));
    check(c_bind, "Couldn't bind");

    // Listen for incomming connection
    int backlog = 3; //number of connections on the backlog
    listen(server_fd, backlog);
    int client_len = sizeof(client);

    while (1){
        client_fd = accept(server_fd, (struct sockaddr*) &client, &client_len);

        check(client_fd, "Could not accept connection");
        printf("New connection from ");
        printf("%s\n", inet_ntoa(client.sin_addr));

        // Create fork for new connection 
        pid_t pid=fork();
        check(pid, "Could not fork");

        if(pid == 0) { // Handle the connection
            close(server_fd);
            close(client_fd);   // Closes child client socket fd before exec

            char* arguments[5] = {"bash",  "--rcfile", "~/.bashrc", "-s", NULL};
            execvp(arguments[0], arguments);
        } else {
            close(client_fd); // Closes parent child socket
        }
    }
    shutdown(server_fd, SHUT_RD);
}



int main(int argc, char** argv){
    // "listen, connect"
    if (argc == 3 && strcmp("listen", argv[1]) == 0)
    {
        int port = atop(argv[2]);
        if(port == 0) return printf("Bad Port\n");
        serverInit_shell(port);
    }
    else if(argc == 4 && strcmp("connect", argv[1]) == 0)
    {
        int port = atop(argv[3]);
        if(port == 0) return printf("Bad Port\n");
        int client_fd = clientInit(port, argv[2]);
        client_shell(client_fd);
    }
    else if(argc == 5 && strcmp("proxy", argv[1]) == 0)
    {
        int listen_port = atop(argv[2]);
        int target_port = atop(argv[4]);

        if(listen_port == 0 || target_port == 0) return printf("Bad Port\n");
        proxy(listen_port, argv[3], target_port);
    }  
    else
    {
        print_usage(argv[2], argv[3]);
    } 
}

