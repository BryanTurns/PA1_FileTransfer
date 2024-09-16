#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>

#define BUFSIZE 1024

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    // Validate usage
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    // (from udp_client.c)
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(hostname);

    // (from udp_client.c)
    /* gethostbyname: get the server's DNS entry */
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    // (from udp_client.c)
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    // Message sending
    /* get a message from the user */
    bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
    n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s", buf);


    // Main loop
    char cmd_buff[500];
    while (1) {
        // Gets command from user
        printf("Enter a command: ");
        fgets(cmd_buff, sizeof(cmd_buff), stdin);
        // 
        char * command = strtok(cmd_buff, " \n");
        printf("%s", cmd_buff);
        if (strcmp(command, "get") == 0) {
            printf("TODO: get\n");
        }
        else if (strcmp(command, "put") == 0) {
            printf("TODO: put\n");
        }
        else if (strcmp(command, "delete") == 0) {
            printf("TODO: delete\n");
        }
        else if (strcmp(command, "ls") == 0) {
            printf("TODO: ls\n");
        }
        else if (strcmp(command, "exit") == 0) {
            printf("Terminating\n");
            return 0;
        }
        else {
            printf("Invalid command. Valid commands are:\nget [file_name]\nput [file_name]\ndelete [file_name]\nls\nexit\n");
        }
    }
}