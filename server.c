#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT "80"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold

void handler(int sig) {
    fprintf(stderr, "Caught signal %d\n", sig);
    exit(EXIT_FAILURE);
}

int main(void) {
    int sockfd, connfd, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever available
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // create a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        // Set SO_REUSEADDR option
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        // bind the socket to the address and port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo); // all done with this structure

    // listen for connections on the socket
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // handle SIGPIPE signal to prevent sudden termination when client closes connection
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1) {
        perror("Failed to ignore SIGPIPE");
        exit(EXIT_FAILURE);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);

        // accept a connection from a client
        if ((connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
            perror("accept");
            continue;
        }

        printf("server: got connection from %s\n", inet_ntoa(clientaddr.sin_addr));

        char message[100] = "Hello, what is your name? ";
        // send a message to the client
        if (send(connfd, message, strlen(message), MSG_NOSIGNAL) == -1) {
            printf("client %d disconnected\n", connfd);
            close(connfd);
            continue;
        }

        char name[20];
        ssize_t num_bytes = recv(connfd, name, sizeof(name)-1, 0);
        if (num_bytes <= 0) {
            printf("client %d disconnected\n", connfd);
            close(connfd);
            continue;
        }

        name[num_bytes] = '\0';
        printf("client %d said their name is %s\n", connfd, name);

        close(connfd); // close the connection
    }

    return 0;
}
