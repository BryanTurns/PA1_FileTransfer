/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define TIMEOUT_S 1
#define TIMEOUT_U 0
#define BUFSIZE 1024
#define ATTEMPTS 5
#define PAYLOADSIZE 512
#define MEMOVER 1

struct packet {
  unsigned int seqnum;
  unsigned int seqlen;
  unsigned int errcode;
  unsigned int payloadlen;
  char payload[PAYLOADSIZE];
};

void unpack(char * buf, struct packet *data);
void pack(char * buf, struct packet *data);
void acknowledge(int sockfd, struct sockaddr_in *clientaddr, int clientlen, int err);
void printpacket(struct packet *data);
int send_data(struct packet *data, char *data_buf, int data_buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err);
void clearbuf(char *buf, int len);
int send_packet(struct packet *data, char *lsdjkdfj, int buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err);


/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE], cwd[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  struct timeval timeout;
  // Get the working directory of server
  if (!getcwd(cwd, sizeof(cwd))) {
    error("Error in getcwd: ");
    exit(1);
  }
  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));
  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);
  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
  error("ERROR on binding");
  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  struct packet *data = (struct packet *) malloc(sizeof(struct packet));
  while (1) {
    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");
    printf("Recieved request\n");
    // Extract buf into data
    unpack(buf, data); 
    // Extract the parts of the command
    char *cmd[2];
    cmd[0] = strtok(data->payload, " ");
    cmd[1] = strtok(NULL, " ");
    // If exit, just acknowledge
    if (strcmp(cmd[0], "exit\n") == 0) {
      acknowledge(sockfd, &clientaddr, clientlen, 0);
    }
    else if (strcmp(cmd[0], "ls\n") == 0) {
      // Acknowledge request
      acknowledge(sockfd, &clientaddr, clientlen, 0);
      // Get directories in cwd
      DIR *d;
      struct dirent *dir;
      d = opendir(cwd);
      int bytes_used = 0, cur_pos = 0;
      // If the cwd exists/accesable NEEDS ERR CHECK
      if (d) {
        // Iterate through all the items in the cwd
        while ((dir = readdir(d)) != NULL) {
          // Iterate over each d_name string and extract it into the buf
          for (int i = 0; i < strlen(dir->d_name)+1; i++) {
            // If buf runs out of memory send an err
            if (!(bytes_used < BUFSIZE)) {
              printf("ERROR: Could not fit directories in buffer\n");
              send_data(data, NULL, 0, sockfd, &clientaddr, &clientlen, &timeout, MEMOVER);
            }
            // Add a char of the name to buf
            buf[cur_pos+i] = dir->d_name[i];
          }
          // Increment the start pos of buf by how much we just wrote
          cur_pos += strlen(dir->d_name)+1;
        }
        closedir(d);
      }
      send_data(data, buf, cur_pos+1, sockfd, &clientaddr, &clientlen, &timeout, 0);
    }
    else if (strcmp(cmd[0], "delete") == 0) {
      acknowledge(sockfd, &clientaddr, clientlen, 0);
      int err = 0;
      char *res;
      cmd[1][strlen(cmd[1])-1] = '\0';
      if (remove(cmd[1]) == 0) 
        res = "Successfully deleted.";
      else {
        res = "Delete failed";
        err++;
      }
      send_data(data, res, strlen(res)+1, sockfd, &clientaddr, &clientlen, &timeout, err);
    }
    else if (strcmp(cmd[0], "put") == 0) {      
      acknowledge(sockfd, &clientaddr, clientlen, 0);
      if (cmd[1][strlen(cmd[1])-1] == '\n')
        cmd[1][strlen(cmd[1])-1] = '\0';
      

      int n_packets = 2, n = 0;
      char *res;

      // Open the file
      FILE *fptr = fopen(cmd[1], "wb+");
      if (!fptr) {
        printf("Could not open file\n");
        continue;
      }
      // Wait for first data packet
      recvfrom(sockfd, buf, BUFSIZE, 0, &clientaddr, &clientlen);
      acknowledge(sockfd, &clientaddr, clientlen, 0);
      unpack(buf, data);
      n_packets = data->seqlen;
      n = fwrite(data->payload, sizeof(char), data->payloadlen, fptr);

      // Write the  packets in
      for (int i = 1; i < n_packets; i++) {
        // consider adding timeout?
        recvfrom(sockfd, buf, BUFSIZE, 0, &clientaddr, &clientlen);
        acknowledge(sockfd, &clientaddr, clientlen, 0);
        unpack(buf, data);
        n = fwrite(data->payload, sizeof(char), data->payloadlen, fptr);
        if (n < 0) {
          printf("File I/O failed\n");
          break;
        }
      }
      fclose(fptr);
      printf("File Transfered to Server\n");
    }
    else if (strcmp(cmd[0], "get") == 0) {
      acknowledge(sockfd, &clientaddr, clientlen, 0);
      char send_buf[PAYLOADSIZE];

      cmd[1][strlen(cmd[1])-1] = '\0';
      FILE *fptr = fopen(cmd[1], "rb");
      if (!fptr) {
        printf("Unable to open files\n");
        continue;
      }
      // Determine file size
      struct stat st;
      if (stat(cmd[1], &st)) {
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
        // Send packet
        if (send_packet(data, send_buf, 0, sockfd, &clientaddr, &clientlen, &timeout, 0)) {
          printf("Unable to send data\n");
          continue;
        }
    }
    fclose(fptr);
  }
}}


// Convert from network to host
void unpack(char *buf, struct packet *data) {
  data->seqnum = ntohs(buf[0] + (buf[1] << 8));
  data->seqlen = ntohs(buf[2] + (buf[3] << 8));
  data->errcode = ntohs(buf[4] + (buf[5] << 8));
  data->payloadlen = ntohs(buf[6] + (buf[7] << 8));
  for (int i = 0; i < data->payloadlen; i++) {
    data->payload[i] = buf[8+i];
  } 
}

// Convert from host to network
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
      // If the payloadlen = 0 it is an ack packet so the server recieved our msg
      if (data->payloadlen == 0) {
        success = 1;
        break;
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

void acknowledge(int sockfd, struct sockaddr_in *clientaddr, int clientlen, int err) {
  struct packet *data = (struct packet *)malloc(sizeof(struct packet));
  char buf[BUFSIZE];
  // Construct packet
  data->seqnum = 0;
  data->seqlen = 1;
  data->errcode = err;
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


int send_packet(struct packet *data, char *lsdjkdfj, int buf_len, int sockfd, struct sockaddr_in *serveraddr, int *serverlen, struct timeval *timeout, int err) {
  char buf[BUFSIZE];
  int fail_count = 0, n = 0, success = 0;
  pack(buf, data);
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

void clearbuf(char *buf, int len) {
  for (int i = 0; i < len; i++)
    buf[i] = 0;
}