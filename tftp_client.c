#include "definitions.h"

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int checkTime(int socketID, char *buf, 

	struct sockaddr_storage servSock, socklen_t socketLen) {

	fd_set f;
	int n;
	struct timeval t;
	FD_ZERO(&f);
	FD_SET(socketID, &f);
	t.tv_sec = TIME_OUT;
	t.tv_usec = 0;

	n = select(socketID+1, &f, NULL, NULL, &t);

	if (n == -1) {
		printf("Timing error.\n");
		return -1; 

	} else if (n == 0){
		printf("Timed out.\n");
		return -2; 
	}

	return recvfrom(socketID, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&servSock, &socketLen);
}

int main(int argc, char* argv[]){
	
	int rv;
	int n;
	int socketID;
	socklen_t socketLen;
	char buf[MAXBUFLEN];
	char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage servSock;
	struct addrinfo hints, *servinfo, *p;
		
	if(argc != 4) {
		fprintf(stderr,"USAGE: tftp_c [GET/PUT] [server] [file name]\n");
		exit(1);
	}

	char *server = argv[2];
	char *file = argv[3]; 
	

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if((rv = getaddrinfo(server, SERVERPORT, &hints, &servinfo)) != 0){
		fprintf(stderr, "Client cannot access port: %s\n", gai_strerror(rv));
		return 1;
	}
	
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((socketID = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("Client failed to create socket.");
			continue;
		}
		break;
	}
	if(p == NULL){
		fprintf(stderr, "Client failed to bind socket\n");
		return 2;
	}

	if(strcmp(argv[1], "GET") == 0 || strcmp(argv[1], "get") == 0) {
		char *message = make_rrq(file);
		char prevm[MAXBUFLEN];
		strcpy(prevm, "");
		char prevAck[10];
		strcpy(prevAck, message);
		int c;

		if((n = sendto(socketID, message, strlen(message), 0, p->ai_addr, p->ai_addrlen)) == -1){
			perror("Client failed to send packet.");
			exit(1);
		}
		printf("Client sent %d bytes to %s\n", n, server);

		char filename[MAX_FILENAME_LEN];
		strcpy(filename, file);
		strcat(filename, "_client");

		FILE *fp = fopen(filename, "wb");
		if(fp == NULL){ //ERROR CHECKING
			fprintf(stderr,"Client failed to file %s\n", filename);
			exit(1);
		}
		
		do{
			socketLen = sizeof servSock;
			if ((n = recvfrom(socketID, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&servSock, &socketLen)) == -1) {
				perror("Client failed to recieve packet.");
				exit(1);
			}
			printf("Client recieved packet from %s\n", inet_ntop(servSock.ss_family, 
				get_in_addr((struct sockaddr *)&servSock), s, sizeof s));
			printf("CLIENT: packet is %d bytes long\n", n);
			buf[n] = '\0';
			printf("CLIENT: packet contains \"%s\"\n", buf);

			if(buf[0]=='0' && buf[1]=='5'){
				fprintf(stderr, "Client recieved error packet: %s\n", buf);
				exit(1);
			}

			if(strcmp(buf, prevm) == 0){
				sendto(socketID, prevAck, strlen(prevAck), 0, (struct sockaddr *)&servSock, socketLen);
				continue;
			}

			c = strlen(buf+4);
			fwrite(buf+4, sizeof(char), c, fp);
			strcpy(prevm, buf);

			char block[3];
			strncpy(block, buf+2, 2);
			block[2] = '\0';
			char *t_msg = make_ack(block);

			if((n = sendto(socketID, t_msg, strlen(t_msg), 0,
			 p->ai_addr, p->ai_addrlen)) == -1){
				perror("Client failed to send acknowledgement.");
				exit(1);
			}
			printf("Client sent %d bytes\n", n);
			strcpy(prevAck, t_msg);

		} while(c == MAX_READ_LEN);

		printf("Successfully wrote to file %s\n", filename);
		fclose(fp);

	} else if(strcmp(argv[1], "PUT") == 0 || strcmp(argv[1], "put") == 0){
		//WRITE TO FILE
		char *message = make_wrq(file);
		char *prevMsg;
		if((n = sendto(socketID, message, strlen(message), 0, 
			p->ai_addr, p->ai_addrlen)) == -1){
			perror("Client failed to sendto server.");
			exit(1);
		}
		printf("Client sent %d bytes to %s\n", n, server);
		prevMsg = message;

		int times;
		socketLen = sizeof servSock;
		for(times=0;times<=MAX_TRIES;++times){
			if(times == MAX_TRIES){// reached max no. of tries
				printf("Client Timed out.\n");
				exit(1);
			}

			n = checkTime(socketID, buf, servSock, socketLen);
			if(n == -1){//error
				perror("Client failed to recieve packet.");
				exit(1);
			} else if (n == -2){//timeout
				printf("Client try %d\n", times+1);
				int temp_bytes;
				if((temp_bytes = sendto(socketID, prevMsg, strlen(prevMsg), 0, p->ai_addr, p->ai_addrlen)) == -1){
					perror("Client failed to send/recieve acknowledgement");
					exit(1);
				}
				printf("Client sent %d bytes\n", temp_bytes);
				continue;
			} else { 
				break;
			}
		}
		printf("Client recieved packet of size %d bytes  from %s\n", n, inet_ntop(servSock.ss_family, 
			get_in_addr((struct sockaddr *)&servSock), s, sizeof s));
		buf[n] = '\0';
		printf("Packet contains \"%s\"\n", buf);

		if(buf[0]=='0' && buf[1]=='4'){
			FILE *fp = fopen(file, "rb");
			if(fp == NULL || access(file, F_OK) == -1){
				fprintf(stderr,"File %s does not exist\n", file);
				exit(1);
			}

			int block = 1;
			fseek(fp, 0, SEEK_END);
			int total = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			int remaining = total;

			if (remaining == 0) {
				++remaining;
			} else if (remaining%MAX_READ_LEN == 0) {
				--remaining;
			}

			while(remaining>0){

				int temp_bytes;
				char temp[MAX_READ_LEN+5];

				if(remaining>MAX_READ_LEN){
					fread(temp, MAX_READ_LEN, sizeof(char), fp);
					temp[MAX_READ_LEN] = '\0';
					remaining -= (MAX_READ_LEN);
				} else {
					fread(temp, remaining, sizeof(char), fp);
					temp[remaining] = '\0';
					remaining = 0;
				}

				//SENDING FILE - DATA PACKET
				char *t_msg = make_data_pack(block, temp);
				if((n = sendto(socketID, t_msg, strlen(t_msg), 0, p->ai_addr, p->ai_addrlen)) == -1){
					perror("Client falied to sendto server");
					exit(1);
				}
				printf("Client sent %d bytes to %s\n", n, server);
				prevMsg = t_msg;

				int times;
				for(times=0;times<=MAX_TRIES;++times){
					if(times == MAX_TRIES){
						printf("Client timed out.\n");
						exit(1);
					}

					n = checkTime(socketID, buf, servSock, socketLen);
					if(n == -1){//error
						perror("Client recvfrom failed");
						exit(1);
					} else if(n == -2){
						printf("CLIENT: try %d\n", times+1);
						if((temp_bytes = sendto(socketID, prevMsg, strlen(prevMsg), 0, p->ai_addr, p->ai_addrlen)) == -1){
							perror("Client failed to recieve acknowledgement");
							exit(1);
						}
						printf("CLIENT: sent %d bytes AGAIN\n", temp_bytes);
						continue;
					} else { 
						break;
					}
				}
				printf("Client recieved packet of size %d bytes from %s\n", n, inet_ntop(servSock.ss_family, get_in_addr((struct sockaddr *)&servSock), s, sizeof s));
				buf[n] = '\0';
				printf("Packet contains \"%s\"\n", buf);

				if(buf[0]=='0' && buf[1]=='5'){
					fprintf(stderr, "Client recieved error packet: %s\n", buf);
					exit(1);
				}
				++block;
				if(block>MAX_PACKETS)
					block = 1;
			}
			
			fclose(fp);
		} else {
			fprintf(stderr,"Client was expecting ACK but got: %s\n", buf);
			exit(1);
		}
	} else { 
		fprintf(stderr,"USAGE: ./tftp_c [GET/PUT] [server] [file name]\n");
		exit(1);
	}

	freeaddrinfo(servinfo);
	close(socketID);
	
	return 0;
}