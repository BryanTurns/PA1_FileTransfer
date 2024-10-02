/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define ATTEMPTS 5
#define TIMEOUT_S 1
#define TIMEOUT_U 0
#define PAYLOADSIZE 512

struct packet {
  int seqnum;
  int seqlen;
  int errcode;
  int payloadlen;
  char payload[512];
};


void pack(char * buf, struct packet *data);
void unpack(char *buf, struct packet *data);
int send_data(struct packet *data, char *data_buf, int data_buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err);
void printpacket(struct packet *data);
void acknowledge(int sockfd, struct sockaddr_in *clientaddr, int clientlen);
void clearbuf(char *buf, int len);
int send_packet(struct packet *data, char *lsdjkdfj, int buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err);
// int do_or_timeout(struct timespec *max_wait);
/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
};


int main(int argc, char **argv) {
  int sockfd, portno, n;
  int serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *hostname;
  char buf[BUFSIZE], netbuf[BUFSIZE], cwd[BUFSIZE];
  struct timeval timeout;
  // Get the current working directory of program
  if (!getcwd(cwd, sizeof(cwd))) {
    error("Error in getcwd: ");
    exit(1);
  }
  /* check command line arguments */
  if (argc != 3) {
    fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
    exit(0);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  
  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */
  serverlen = sizeof(serveraddr);
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
	(char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  while (1){
    /* get a message from the user */
    bzero(buf, PAYLOADSIZE);
    printf("Please enter a command: ");
    fgets(buf, PAYLOADSIZE, stdin);
    char fullcmd[PAYLOADSIZE];
    strcpy(fullcmd, buf);
    char *cmds[2];
    cmds[0] = strtok(buf, " ");
    if (cmds[0] != NULL) {
      cmds[1] = strtok(NULL, " ");
      if (cmds[1] != NULL && strtok(NULL, " ")) {
        printf("Invalid command, too many arguments\n");
        continue;
      }
    }
      
    
    // printf("0: %s", cmds[0]);
    // printf("1: %s", cmds[1]);
    
    // Create data struct to store data to send/recieve
    struct packet *data = (struct packet *)malloc(sizeof(struct packet));
    // exit: If the server acknowledges, terminate
    if (strcmp(cmds[0], "exit\n") == 0) {
      serverlen = sizeof(serveraddr);
      if(!send_data(data, cmds[0], strlen(cmds[0])+1, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
        free(data);
        exit(0);
      }
      printf("Exit failed\n");
    }
    // ls: 
    else if (strcmp(cmds[0], "ls\n") == 0 ) {
      // Send ls request to server
      
      if(send_data(data, cmds[0], strlen(cmds[0])+1, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
        printf("Error: ls failed\n");
        continue;
      }
      // Get result from server & acknowledge
      recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, serverlen);
      acknowledge(sockfd, &serveraddr, serverlen);
      unpack(buf, data);
      // Output ls to console
      for (int i = 0; i < data->payloadlen-1; i++) {
        if (data->payload[i] == '\0')
          data->payload[i] = '\n';
      }
      printf("%s", data->payload);
    }
    else if (strcmp(cmds[0], "delete") == 0) {
      char send_buf[PAYLOADSIZE];
      int i;
      // Check if cmd is valid
      if (cmds[1] == NULL) {
        printf("Invalid command\n");
        continue;
      }
      // Construct cmd payload
      if (strlen(cmds[0]) + strlen(cmds[1])+2 < PAYLOADSIZE) {
        strcpy(send_buf, cmds[0]);
        send_buf[strlen(cmds[0])] = ' ';
        strcpy((send_buf + strlen(cmds[0])+1), cmds[1]);
      }
      // If the cmd is too big throw an err
      else {
        printf("Error: Command exceeds memory\n");
        continue;
      }

      if(send_data(data, send_buf, strlen(cmds[0]) + strlen(cmds[1])+2, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
        printf("Delete failed\n");
        continue;
      }
      recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
      acknowledge(sockfd, &serveraddr, serverlen);
      unpack(buf, data);
      printf("Server: %s\n", data->payload);
    }
    else if (strcmp(cmds[0], "put") == 0) {
      printf("TEST: %s\n", fullcmd);
      char send_buf[PAYLOADSIZE];
      int i, n;
      // Check if cmd is valid
      if (cmds[1] == NULL) {
        printf("Invalid command\n");
        continue;
      }
      // Send the command
      if(send_data(data, fullcmd, strlen(fullcmd)+1, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
        printf("Put failed\n");
        continue;
      }
      // Open the file
      cmds[1][strlen(cmds[1])-1] = '\0';
      FILE *fptr = fopen(cmds[1], "rb");
      if (!fptr) {
        printf("Unable to open files\n");
        continue;
      }
      // Determine file size
      struct stat st;
      if (stat(cmds[1], &st)) {
        printf("Unable to determine filesize\n");
        continue;
      }
      int fsize = st.st_size;
      // Determine num_packets
      int num_packets = fsize / PAYLOADSIZE;
      if (fsize % PAYLOADSIZE)
        num_packets++;
      int remaining_bytes = fsize % PAYLOADSIZE;
      // Send the data 
      for (int i = 0; i < num_packets; i++) {
        // Handle extra bytes
        if (i == num_packets - 1  && remaining_bytes)
          data->payloadlen = remaining_bytes;
        else 
          data->payloadlen = PAYLOADSIZE;
        // Read from the file
        n = fread(send_buf, sizeof(char), data->payloadlen, fptr);
        if (n < 0) {
          printf("Unable to read file\n");
          continue;
        }
        
        // Construct packet
        data->errcode = 0;
        data->seqlen = num_packets;
        data->seqnum = i;
        
        for (int j = 0; j < strlen(send_buf)+1; j++) {
          data->payload[j] = send_buf[j];
        }
        printpacket(data);
        if (send_packet(data, send_buf, 0, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
          printf("Unable to send data\n");
          continue;
        }
        
      }
      



    }
    else if (strcmp(cmds[0], "get") == 0) {
      printf("GET\n");
      char send_buf[PAYLOADSIZE];
      int i, n;
      // Check if cmd is valid
      if (cmds[1] == NULL) {
        printf("Invalid command\n");
        continue;
      }
      // Eliminate newline at end of input if it exists
      if (cmds[1][strlen(cmds[1])-1] == '\n')
        cmds[1][strlen(cmds[1])-1] = '\0';

      // Open file for wtiting
      FILE *fptr = fopen(cmds[1], "wb+");
      if (!fptr) {
        printf("Could not open file\n");
        continue;
      }

      // Send the command
      if(send_data(data, fullcmd, strlen(fullcmd)+1, sockfd, &serveraddr, &serverlen, &timeout, 0)) {
        printf("Get failed\n");
        continue;
      }
      printf("TEST\n");

      // Get the first data packet
      // Maybe timeout?
      recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
      acknowledge(sockfd, &serveraddr, serverlen);
      unpack(buf, data);
      int n_packets = data->seqlen;
      n = fwrite(data->payload, sizeof(char), data->payloadlen, fptr);
      
      for (int i = 1; i < n_packets; i++) {
        recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        acknowledge(sockfd, &serveraddr, serverlen);
        unpack(buf, data);
        n = fwrite(data->payload, sizeof(char), data->payloadlen, fptr);
        if (n < 0) {
          printf("File I/O failed\n");
          break;
        }
      }

      fclose(fptr);
    }
    else 
      printf("Command not recognized\n");
  }
}

// Convert from network to host
// void unpack(char *buf, struct packet *data) {
//   data->seqnum = buf[1] | (buf[0] << 8);
//   data->seqlen = buf[3] | (buf[2] << 8);
//   data->errcode = buf[5] | (buf[4] << 8);
//   data->payloadlen = buf[7] | (buf[6] << 8);
//   for (int i = 0; i < data->payloadlen; i++) {
//     data->payload[i] = buf[8+i];
//   } 
// }
void unpack(char *buf, struct packet *data) {
  data->seqnum = ntohs(buf[0] + (buf[1] << 8));
  data->seqlen = ntohs(buf[2] + (buf[3] << 8));
  data->errcode = ntohs(buf[4] + (buf[5] << 8));
  data->payloadlen = ntohs(buf[6] + (buf[7] << 8));
  for (int i = 0; i < data->payloadlen; i++) {
    data->payload[i] = buf[8+i];
  } 
}

// // Convert from host to network
// void pack(char *buf, struct packet *data) {
//   clearbuf(buf, BUFSIZE);
//   buf[0] = ((data->seqnum & 0xFF00) >> 8);
//   buf[1] = data->seqnum & 0x00FF;
//   buf[2] = ((data->seqlen & 0xFF00) >> 8);
//   buf[3] = data->seqlen & 0x00FF;
//   buf[4] = ((data->errcode & 0xFF00) >> 8);
//   buf[5] = data->errcode & 0x00FF;
//   buf[6] = ((data->payloadlen & 0xFF00) >> 8);
//   buf[7] = data->payloadlen & 0x00FF;
//   for (int i = 0; i < data->payloadlen; i++) {
//     buf[8+i] = data->payload[i];
//   }
// }

void pack(char *buf, struct packet *data) {
  clearbuf(buf, BUFSIZE);
  buf[0] =  htons(data->seqnum) & 0x00FF;
  buf[1] = (htons(data->seqnum) & 0xFF00) >> 8;
  buf[2] = htons(data->seqlen) & 0x00FF;
  buf[3] = (htons(data->seqlen) & 0xFF00) >> 8;
  buf[4] = htons(data->errcode) & 0x00FF;
  buf[5] = (htons(data->errcode) & 0xFF00) >> 8;
  buf[6] = (htons(data->payloadlen) & 0x00FF);
  buf[7] = (htons(data->payloadlen) & 0xFF00) >> 8;
  for (int i = 0; i < data->payloadlen; i++) {
    buf[8+i] = data->payload[i];
  }
}
int send_packet(struct packet *data, char *lsdjkdfj, int buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err) {
  char buf[BUFSIZE];
  int fail_count = 0, n = 0, success = 0;
  pack(buf, data);
  unpack(buf, data);
  printf("TESTING PACKET: \n");
  printpacket(data);
  // Track whether the packet gets acknowledged
  fail_count = 0;
  success = 0;
  // Attempt to send packet
  while (fail_count < ATTEMPTS) {
    // Send the data to the server
    n = sendto(sockfd, buf, BUFSIZE, 0, serveraddr, *serverlen);
    if (n < 0) 
      error("ERROR in sendto");      
    // Set timeout
    timeout->tv_usec = TIMEOUT_U;
    timeout->tv_sec = TIMEOUT_S;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(*timeout));
    // Get the acknowledgment
    n = recvfrom(sockfd, buf, BUFSIZE, 0, serveraddr, serverlen);
    if (n < 0) {
      fail_count++;
      continue;
    }
    // Remove timeout
    timeout->tv_sec = 0;
    timeout->tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(*timeout));
    // Check if ack recieved in time
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      fail_count++;
      continue;
    }
    // Check if ack recieved at all
    else if (n < 0) 
      error("ERROR in recvfrom");
    // extract buf into data
    unpack(buf, data);
    // printpacket(data);
    // If the payloadlen = 0 it is an ack packet so the server recieved our msg
    if (data->payloadlen == 0 && data->errcode == 0) {
      success = 1;
      break;
    }
    else if (data->errcode != 0) {
      printf("Error in server\n");
      return 1;
    }
    fail_count++;
  }
  // If the server never acknowledges packet
  if (!success) {
    printf("Failed to recieve acknowledgment\n");
    return 1;
  }
}

int send_data(struct packet *data, char *data_buf, int data_buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err) {
  int fail_count = 0, n = 0, success = 0;
  char buf[BUFSIZE];
  // Calculate number of packets and how many bytes are left over in final packet
  int final_payload_size = data_buf_len % PAYLOADSIZE;
  int num_packets = data_buf_len / PAYLOADSIZE;
  if (final_payload_size != 0) 
    num_packets++;

  // For each packet
  for (int pnum = 0; pnum < num_packets; pnum++) {
    // If it's the final packet, it should have the remaining bytes as its size, otherwize just the max size
    if (final_payload_size > 0 && pnum == num_packets - 1) 
      data->payloadlen = final_payload_size;
    else 
      data->payloadlen = PAYLOADSIZE;
    // Construct the packet
    data->seqnum = pnum;
    data->seqlen = num_packets;
    data->errcode = err;
    // Startpos is where this packet should begin reading from in the data_buf
    int startpos = pnum*PAYLOADSIZE;
    for (int i = 0; i < data->payloadlen; i++) 
      data->payload[i] = data_buf[startpos+i];
    pack(buf, data);
    // printpacket(data);
    // Track whether the packet gets acknowledged
    fail_count = 0;
    success = 0;
    // Attempt to send packet
    while (fail_count < ATTEMPTS) {
      // Send the data to the server
      n = sendto(sockfd, buf, BUFSIZE, 0, serveraddr, *serverlen);
      if (n < 0) 
        error("ERROR in sendto");      
      // Set timeout
      timeout->tv_usec = TIMEOUT_U;
      timeout->tv_sec = TIMEOUT_S;
      setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(*timeout));
      // Get the acknowledgment
      n = recvfrom(sockfd, buf, BUFSIZE, 0, serveraddr, serverlen);
      if (n < 0) {
        fail_count++;
        continue;
      }
      // Remove timeout
      timeout->tv_sec = 0;
      timeout->tv_usec = 0;
      setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(*timeout));
      // Check if ack recieved in time
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        fail_count++;
        continue;
      }
      // Check if ack recieved at all
      else if (n < 0) 
        error("ERROR in recvfrom");
      // extract buf into data
      unpack(buf, data);
      // printpacket(data);
      // If the payloadlen = 0 it is an ack packet so the server recieved our msg
      if (data->payloadlen == 0 && data->errcode == 0) {
        success = 1;
        break;
      }
      else if (data->errcode != 0) {
        printf("Error in server\n");
        return 1;
      }
      fail_count++;
    }
    // If the server never acknowledges packet
    if (!success) {
      printf("Failed to recieve acknowledgment\n");
      return 1;
    }
  }
  return 0;
}


void printpacket(struct packet *data) {
  printf("Packet %d/%d: \n", data->seqnum+1, data->seqlen);
  printf("\tErr Code: %d\n", data->errcode);
  printf("\tPayload Len: %d\n", data->payloadlen);
  // printf("\tPayload: %s\n", data->payload);
}

void acknowledge(int sockfd, struct sockaddr_in *clientaddr, int clientlen) {
  struct packet *data = (struct packet *)malloc(sizeof(struct packet));
  char buf[BUFSIZE];
  // Construct packet
  data->seqnum = 0;
  data->seqlen = 1;
  data->errcode = 0;
  data->payloadlen = 0;
  data->payload[0] = '\0';
  // Pack the packet
  pack(buf, data);   
  // Send the acknowledgment
  int n = sendto(sockfd, buf, BUFSIZE, 0, clientaddr, clientlen);
  if (n < 0) 
    error("ERROR in sendto");
  // Cleanup memory
  free(data);
}

void clearbuf(char *buf, int len) {
  for (int i = 0; i < len; i++)
    buf[i] = 0;
}