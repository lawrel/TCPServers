#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>
#include "chatHeader.h"

int tftp_handler(){

  //char portno[6];
  //sprintf(portno, "%d", port);
	int sockID;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int n;
	struct sockaddr_storage cliSock;
	char buf[MAX_BUFFER];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Server failed to find client: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockID = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("Server failed to create socket.");
			continue;
		}
		if (bind(sockID, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockID);
			perror("Server failed to bind to socket.");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "Server failed to bind to socket.\n");
		return 2;
	}
	freeaddrinfo(servinfo);

	printf("Server is listening on port %s\n",SERVERPORT);

	addr_len = sizeof cliSock;
	if ((n = recvfrom(sockID, buf, MAX_BUFFER-1 , 0, (struct sockaddr *)&cliSock, &addr_len)) == -1) {
		perror("Server failed to recieve packet.");
		exit(1);
	}
	printf("Server got packet of size %d from %s\n", n,
		inet_ntop(cliSock.ss_family, get_addr((struct sockaddr *)&cliSock), s, sizeof s));
	buf[n] = '\0';
	printf("Packet contains \"%s\"\n", buf);

	if(buf[0] == '0' && buf[1] == '1'){

		char filename[MAX_FILENAME_LEN];
		strcpy(filename, buf+2);

		FILE *fp = fopen(filename, "rb");
		if(fp == NULL || access(filename, F_OK) == -1){ //SENDING ERROR PACKET - FILE NOT FOUND
			fprintf(stderr,"File '%s' does not exist, sending error packet\n", filename);
			char *e_msg = make_err("02", "ERROR_FILE_NOT_FOUND");
			printf("%s\n", e_msg);
			sendto(sockID, e_msg, strlen(e_msg), 0, (struct sockaddr *)&cliSock, addr_len);
			exit(1);
		}


		int block = 1;
		fseek(fp, 0, SEEK_END);
		int total = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		int remaining = total;
		if(remaining == 0)
			++remaining;
		else if(remaining%MAX_READ_LEN == 0)
			--remaining;

		char temp[MAX_READ_LEN+5];
		while(remaining>0){

			if(remaining>MAX_READ_LEN){
				fread(temp, MAX_READ_LEN, sizeof(char), fp);
				temp[MAX_READ_LEN] = '\0';
				remaining -= (MAX_READ_LEN);
			} else {
				fread(temp, remaining, sizeof(char), fp);
				temp[remaining] = '\0';
				remaining = 0;
			}

			char *t_msg = make_data(block, temp);
			if((n = sendto(sockID, t_msg, strlen(t_msg), 0, (struct sockaddr *)&cliSock, addr_len)) == -1){
				perror("Server failed to send acknowledgement.");
				exit(1);
			}
			printf("Server sent %d bytes\n", n);

			//WAITING FOR ACKNOWLEDGEMENT - DATA PACKET
			int times;
			for(times=0;times<=MAX_TRIES;++times){
				if(times == MAX_TRIES){
					printf("Server timed out.\n");
					exit(1);
				}

				n = checkTime(sockID, buf, cliSock, addr_len);
				if(n == -1){
					perror("Server failed to recieve packet.");
					exit(1);
				} else if(n == -2){
					printf("Server try %d\n", times+1);
					int temp_bytes;
					if((temp_bytes = sendto(sockID, t_msg, strlen(t_msg), 0, p->ai_addr, p->ai_addrlen)) == -1){
						perror("Server failed to send acknowledgement.");
						exit(1);
					}
					printf("Server sent %d bytes\n", temp_bytes);
					continue;
				} else {
					break;
				}
			}
			printf("Server got packet of size %d from %s\n", n,
				inet_ntop(cliSock.ss_family, get_addr((struct sockaddr *)&cliSock), s, sizeof s));
			buf[n] = '\0';
			printf("Packet contains \"%s\"\n", buf);

			++block;
			if(block>MAX_PACKETS)
				block = 1;
		}
		fclose(fp);
	} else if(buf[0] == '0' && buf[1] == '2'){

		char *message = make_ack("00");
		char prevMsg[MAX_BUFFER];strcpy(prevMsg, buf);
		char prevAck[10];strcpy(prevAck, message);
		if((n = sendto(sockID, message, strlen(message), 0, (struct sockaddr *)&cliSock, addr_len)) == -1){
			perror("Server failed to send acknowledgement.");
			exit(1);
		}
		printf("Server sent %d bytes\n", n);

		char filename[MAX_FILENAME_LEN];
		strcpy(filename, buf+2);
		strcat(filename, "_server");

		if(access(filename, F_OK) != -1){ //SENDING ERROR PACKET - DUPLICATE FILE
			fprintf(stderr,"File %s already exists, sending error packet\n", filename);
			char *e_msg = make_err("06", "ERROR_FILE_ALREADY_EXISTS");
			sendto(sockID, e_msg, strlen(e_msg), 0, (struct sockaddr *)&cliSock, addr_len);
			exit(1);
		}

		FILE *fp = fopen(filename, "wb");
		if(fp == NULL || access(filename, W_OK) == -1){ //SENDING ERROR PACKET - ACCESS DENIED
			fprintf(stderr,"File %s access denied, sending error packet\n", filename);
			char *e_msg = make_err("05", "ERROR_ACCESS_DENIED");
			sendto(sockID, e_msg, strlen(e_msg), 0, (struct sockaddr *)&cliSock, addr_len);
			exit(1);
		}

		int c;
		do{
			if ((n = recvfrom(sockID, buf, MAX_BUFFER-1 , 0, (struct sockaddr *)&cliSock, &addr_len)) == -1) {
				perror("Server failed to recieve packet");
				exit(1);
			}
			printf("Server got packet of size %d bytes from %s\n", n,
				inet_ntop(cliSock.ss_family, get_addr((struct sockaddr *)&cliSock), s, sizeof s));
			buf[n] = '\0';
			printf("Packet contains \"%s\"\n", buf);

			if(strcmp(buf, prevMsg) == 0){
				sendto(sockID, prevAck, strlen(prevAck), 0, (struct sockaddr *)&cliSock, addr_len);
				continue;
			}

			c = strlen(buf+4);
			fwrite(buf+4, sizeof(char), c, fp);
			strcpy(prevMsg, buf);

			char block[3];
			strncpy(block, buf+2, 2);
			block[2] = '\0';
			char *t_msg = make_ack(block);
			if((n = sendto(sockID, t_msg, strlen(t_msg), 0, (struct sockaddr *)&cliSock, addr_len)) == -1){
				perror("Server failed to send acknowledgement.");
				exit(1);
			}
			printf("Server sent %d bytes\n", n);
			strcpy(prevAck, t_msg);

		} while(c == MAX_READ_LEN);

		printf("Sucessfully wrote to %s \n", filename);
		fclose(fp);

	} else {
		fprintf(stderr,"INVALID REQUEST\n");
		exit(1);
	}

	close(sockID);

	return 0;
}
void * udp_handler(void * arg) {
  struct User user_;
  udp_user temp;
  temp = *(udp_user*) arg;
	user_.oper = 0;
	user_.fd = temp.sockno;
	char buffer[1024];
	char command[20];
	char arg1[20];
	char arg2[20];
	char msg[256];
	char message[512];
	int nc;

  try_read:
  sendto(user_.fd, "Please note, some functions may be unsupported over UDP.\n", 57, 0, (struct sockaddr *)&temp.client, temp.clilen );
  sprintf(msg,"To begin, enter a username using USER <name>\n");
  sendto(user_.fd,msg, strlen(msg),0, (struct sockaddr *)&temp.client, temp.clilen);
  if ((nc = recvfrom(user_.fd, buffer, 1024, 0, (struct sockaddr *)&temp.client, (socklen_t *)&temp.clilen))==-1){
    if(errno == EINTR) {
			goto try_read;
		} else {
			close(user_.fd);
			pthread_exit(NULL);
		}
  }
  if (Parse(buffer, nc, command, arg1, arg2, message)) {
    sendto(user_.fd, "ERROR: Unsupported command format\n", 34, 0, (struct sockaddr *)&temp.client, temp.clilen );
  };
  pthread_exit(NULL);
}
