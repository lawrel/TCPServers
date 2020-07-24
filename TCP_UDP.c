#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#define MAX_BUFFER 1024
typedef struct {
	int fd;
	char * name;
  //int istcp = 0;
} User;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

User * users; //Array of all users
int num_users = 0; //Current number of Users
void prntUsers(User * user) {
  for (int i = 0; i < num_users; i++) {
    printf("[%d , %s]\n",user->fd,user->name);
  }
}
int sortUsers(const void * a, const void * b) {
  const User *u1 = (const User *)a;
  const User *u2 = (const User *)b;
  if (strcmp(u1->name," ")==0&&strcmp(u2->name," ")!=0) {
    return 1;
  } else if (strcmp(u1->name," ")!=0&&strcmp(u2->name," ")==0){
    return -1;
  } else if (strcmp(u1->name," ")==0&&strcmp(u2->name," ")==0) {
    return 0;
  } else {
    return strcmp(u1->name,u2->name);
  }
}
void sendError( User * user, char * msg) {
  pthread_mutex_lock(&mutex);
  if (strcmp(msg,"broadcast")==0) {
    send(user->fd, "ERROR Invalid BROADCAST format\n", 31, 0 );
    printf("CHILD %ld: Sent ERROR (Invalid BROADCAST format)\n" , pthread_self());
  }
  if (strcmp(msg,"msglen")==0) {
    send(user->fd, "ERROR Invalid message length\n", 29,0);
    printf("CHILD %ld: Sent ERROR (Invalid message length)\n" , pthread_self());
  }
  if (strcmp(msg,"loggedin")==0) {
    send(user->fd, "ERROR must be logged in\n", 24, 0 );
  	printf("CHILD %ld: Sent ERROR (must be logged in)\n" , pthread_self());
  }
  if (strcmp(msg,"send")==0) {
    send(user->fd, "ERROR Invalid SEND format\n", 26, 0 );
    printf("CHILD %ld: Sent ERROR (Invalid SEND format)\n" , pthread_self());
  }
  if (strcmp(msg,"badname")==0) {
		send(user->fd, "ERROR Invalid userid\n", 21, 0 );
		printf("CHILD %ld: Sent ERROR (Invalid userid)\n", pthread_self());
  }
  if (strcmp(msg,"already")==0) {
    send( user->fd, "ERROR Already connected\n", 24, 0 );
    printf("CHILD %ld: Sent ERROR (Already connected)\n", pthread_self());
  }
  if (strcmp(msg,"share")==0) {
    send(user->fd, "ERROR Invalid SHARE format\n", 27, 0 );
    printf("CHILD %ld: Sent ERROR (Invalid SHARE format)\n" , pthread_self());
  }
  if (strcmp(msg,"filelen")==0) {
    send(user->fd, "ERROR Invalid file length\n", 26, 0 );
    printf("CHILD %ld: (Sent ERROR Invalid file length)\n" , pthread_self());
  }
  if (strcmp(msg,"unknown")==0) {
    send(user->fd, "ERROR Unknown userid\n", 21, 0 );
    printf("CHILD %ld: Sent ERROR (Unknown userid)\n",pthread_self());

  }
  pthread_mutex_unlock(&mutex);
}

void remove_client(User * user) {
  for (int i = 0; i < num_users; i++) {
    if (user->fd == users[i].fd) {
      for (int temp = i; temp < num_users-1; temp++) {
        pthread_mutex_lock(&mutex);
        users[temp].fd = users[temp+1].fd;
        strcpy(users[temp].name,users[temp+1].name);
        pthread_mutex_unlock(&mutex);
      }
      pthread_mutex_lock(&mutex);
      num_users--;
      pthread_mutex_unlock(&mutex);
    }
  }
}
//Parses given input from USER
int Parse(char* input_, char*** args, char delimiter, int num) {
	int argnum = 0;
	char* input = (char*)calloc((strlen(input_)+1+1), sizeof(char));
	strcpy(input, input_);
	int count = 0;
	char* arg = (char*)calloc(1024, sizeof( char ));
	for (size_t i = 0; i < strlen(input); i++) {
	   int index = strlen(arg);
		if (input[i] == delimiter && count < num) {
			count += 1;
			if (strlen(arg) > 0) {
			   argnum += 1;
				if (*args == NULL) {
					*args = (char**)calloc(argnum, sizeof( char* ));
					if ( *args == NULL )	{
						fprintf( stderr, "ERROR: calloc() failed\n" );
						exit(1);
					}
				}
				/*else, reallocate*/
				else {
					*args = (char**)realloc(*args, argnum*sizeof(char*));
					if ( *args == NULL ) {
						fprintf( stderr, "ERROR: realloc() failed\n" );
						return EXIT_FAILURE;
					}
				}
				(*args)[argnum - 1] = (char*)calloc(strlen(arg)+1, sizeof( char ));
				if ( (*args)[argnum - 1] == NULL ) {
					fprintf( stderr, "ERROR: calloc() failed\n" );
					return EXIT_FAILURE;
				}
				strcpy((*args)[argnum - 1], arg);
				memset(arg, 0, 1024);
			}
		} else {
			arg[index] = input[i];
		}
	}

	if (strlen(arg) > 0) {
		argnum += 1;
		*args = (char**)realloc(*args, argnum*sizeof(char*));
		(*args)[argnum - 1] = (char*)calloc(strlen(arg)+1, sizeof( char ));
		strcpy((*args)[argnum - 1], arg);
	}

	if (argnum > 0) {
		*args = (char**)realloc(*args, (argnum+1)*sizeof(char*));
		(*args)[argnum] = NULL;
	}

	free(input);
	free(arg);
	return argnum;
}
void help(User * user) {
	char msg[256];
	sprintf(msg,"AVAILABLE COMMANDS:\n -LOGIN <username>\n -LOGOUT\n -WHO\n -SHARE <message>\n -BROADCAST <size> <message>\n -HELP \n -SEND <user><size><message>\n -QUIT \n");
	send(user->fd,msg, strlen(msg),0);
}
void login(User* user, char* buffer, char*** in, int argnum) {
  char**args = NULL;
  int n = Parse(buffer,&args,' ',3);
  int repeat_name = 0;
	if (n == 2) {
		args[1][strlen(args[1])-1] = '\0';
		printf("CHILD %ld: Rcvd LOGIN request for userid %s\n", pthread_self(), args[1]);
		if (strlen(args[1]) >= 4 && strlen(args[1]) <= 16) {
			for (int i=0; i < num_users; i++) {
				if (strcmp(users[i].name, args[1]) == 0) {
					repeat_name = 1;
					break;
				}
			}
			if (repeat_name == 0) {
				for (int i=0; i < num_users; i++) {
					if (users[i].fd == user->fd) {
						if (strcmp(users[i].name, " ") == 0) {
							users[i].name = args[1];
							send( user->fd, "OK!\n", 4, 0 );
              pthread_mutex_lock( &mutex );
							strcpy(user->name, args[1]);
              pthread_mutex_unlock( &mutex );
						}	else {
							char a[8] = "already";
              sendError(user,a);
						}
						break;
					}
				}
			} else {
        char a[8] = "already";
        sendError(user,a);
			}
		} else {
      char a[8] = "badname";
      sendError(user,a);
		}
	}	else {
    char a[8] = "badname";
    sendError(user,a);
	}
}
void logout(User* user, char*** in, int argnum) {
  printf("CHILD %ld: Rcvd LOGOUT request\n", pthread_self());
	if (strcmp(user->name, " ") != 0) {
		for (int k = 0 ; k < num_users ; k++ ) {
			if (user->fd == users[k].fd ) {
				pthread_mutex_lock( &mutex );
				send( user->fd, "OK!\n", 4, 0 );
				strcpy(users[k].name, " ");
				strcpy(user->name, " ");
				pthread_mutex_unlock( &mutex );
				break;
			}
		}
	}	else {
    char a[9] = "loggedin";
    sendError(user,a);
	}
}
void who(User* user) {
  char buffer[1024];
  printf("CHILD %ld: Rcvd WHO request\n", pthread_self());
	sprintf(buffer, "OK!\n");
	pthread_mutex_lock( &mutex );
	qsort(users, num_users, sizeof(User), sortUsers);
	for (int i = 0; i < num_users; i++)	{
		if (strcmp(users[i].name, " ") != 0)	{
			sprintf(buffer+strlen(buffer), "%s\n", users[i].name) ;
		} else {
			break;
		}
	}
	send(user->fd, buffer, strlen(buffer), 0);
	pthread_mutex_unlock( &mutex );
}
void share( User* user, char* buffer, int argnum) {
  printf("CHILD %ld: Rcvd SHARE request\n", pthread_self());
  if (strcmp(user->name, " ") != 0) {
  	char** args = NULL;
  	int share_arg_num = Parse(buffer, &args, ' ', 4);
  	int found_recip = 0;
  	int recip_fd = 0;
  	if (share_arg_num == 3) {
  		char* filelen = (char*)calloc(strlen(args[2]), sizeof(char));
  		strcpy(filelen, args[2]);
  		filelen[sizeof(filelen) - 1] = '\0';
  		if (atoi(filelen) > 0) {
  			for (int i = 0; i < num_users; i++) {
  				if (strcmp(users[i].name, args[1]) == 0) {
  					found_recip = 1;
  					recip_fd = users[i].fd;
  					break;
  				}
  			}
  			if (found_recip == 1){
  				int bytes_left = atoi(args[2]);
  				pthread_mutex_lock( &mutex );
  				send(user->fd, "OK!\n", 4, 0);
  				pthread_mutex_unlock( &mutex );
  				sprintf(buffer, "SHARE %s %s", user->name, args[2]);
  				pthread_mutex_lock( &mutex );
  				send(recip_fd, buffer, strlen(buffer), 0);
  				pthread_mutex_unlock( &mutex );
  				while (bytes_left != 0){
  					int n = recv( user->fd, buffer, MAX_BUFFER, 0 );
  					pthread_mutex_lock( &mutex );
  					send(user->fd, "OK!\n", 4, 0);
  					send(recip_fd, buffer, n, 0);
  					pthread_mutex_unlock( &mutex );
  					bytes_left -= n;
  				}
  			} else {
          char a[6] = "share";
          sendError(user,a);
  			}
  		} else {
  			char a[8] = "filelen";
        sendError(user,a);
  			}
  	} else {
  		char a[6] = "share";
      sendError(user,a);
  	}
  } else {
    char a[8] = "unknown";
    sendError(user,a);
  }
}
void Send(User* user,  char* in, int argnum) {
  if (strcmp(user->name, " ") != 0) {
		char** send_args = NULL;
		int valid_format = 0;
		int send_arguments_num = Parse(in, &send_args, ' ', 2);
		if (send_arguments_num == 3) {
			char** msgarg = NULL;
			int args = Parse(send_args[2], &msgarg, '\n', 1);
			if (args == 2) {
				valid_format = 1;
			} else if (args == 1) {
				if (send_args[2][strlen(send_args[2])-1] == '\n') {
					valid_format = 1;
				}
			}
			if (valid_format == 1) {
				printf("CHILD %ld: Rcvd SEND request to userid %s\n", pthread_self(), send_args[1]);
				int found = 0;
				int fd_found = 0;
				for (int i = 0; i < num_users; i++){
					if (strcmp(users[i].name, send_args[1]) == 0){
						found = 1;
						fd_found = users[i].fd;
						break;
					}
				}
				if (found == 1) {
				  if (atoi(msgarg[0]) >= 1 && atoi(msgarg[0]) <= 990) {
						pthread_mutex_lock( &mutex );
						send(user->fd, "OK!\n", 4, 0);
						pthread_mutex_unlock( &mutex );
						int bytes_left = atoi(msgarg[0]);
						char* send_buffer = (char*)calloc( (strlen(msgarg[0])+9+strlen(user->name) + atoi(msgarg[0])), sizeof(char));
						sprintf(send_buffer, "FROM %s %s ",user->name, msgarg[0]);
						if (args == 2) {
							sprintf(send_buffer+strlen(send_buffer), "%s", msgarg[1]);
							bytes_left -= strlen(msgarg[1]);
						}
						while ( bytes_left > 0) {
              char buffer[1024];
							int n = recv(user->fd, buffer, MAX_BUFFER, 0);
							bytes_left -= n;
							sprintf(send_buffer+strlen(send_buffer), "%s", buffer);
						}
						pthread_mutex_lock( &mutex );
						send(fd_found, send_buffer, strlen(send_buffer), 0);
						pthread_mutex_unlock( &mutex );
						free(send_buffer);
					} else {
					   char a[7] = "msglen";
             sendError(user,a);
					}
				} else {
				char a[8] = "unknown";
        sendError(user,a);
				}
			} else {
        char a[5] = "send";
        sendError(user,a);
			}
		} else {
			char a[5] = "send";
      sendError(user,a);
		}
	} else {
		char a[9] = "loggedin";
    sendError(user,a);
	}
}
void broadcast(User* user,  char*** in, int argnum) {
  printf("CHILD %ld: Rcvd BROADCAST request\n", pthread_self());
  if (strcmp(user->name, " ") != 0) {
  	if (argnum == 2) {
  		char** args = NULL;
  		int num = Parse((*in)[1], &args, '\n', 1);
  		int valid_format = 0;
  		if (num == 2) {
  			valid_format = 1;
  		}
  		if (num == 1) {
  			if ((*in)[1][strlen((*in)[1])-1] == '\n')	{
  				valid_format = 1;
  			}
  		}
  		if (valid_format == 1) {
  			if (atoi(args[0]) >= 1 && atoi(args[0]) <= 990) {
  				pthread_mutex_lock( &mutex );
  				send(user->fd, "OK!\n", 4, 0);
  				pthread_mutex_unlock( &mutex );
  				char* send_buffer = (char*)calloc((strlen(args[0])+ 9 +strlen(user->name) + atoi(args[0])), sizeof(char));
  				int bytes_left = atoi(args[0]);
  				sprintf(send_buffer, "FROM %s %s ", user->name, args[0]);
  				if (num == 2) {
  					sprintf(send_buffer+strlen(send_buffer), "%s", args[1]);
  					bytes_left -= strlen(args[1]);
  				}
  				while ( bytes_left > 0) {
            char buffer[1024];
  				  int n = recv(user->fd, buffer, 1024, 0);
  				  bytes_left -= n;
  				  sprintf(send_buffer+strlen(send_buffer), "%s", buffer);
  				}
  				for (int k = 0 ; k < num_users ; k++ ) {
  					if ( strcmp(users[k].name, " ") != 0 ) {
  						pthread_mutex_lock( &mutex );
  						send(users[k].fd, send_buffer, strlen(send_buffer), 0);
  						pthread_mutex_unlock( &mutex );
  					}
  				}
  				free(send_buffer);
  			} else {
  				char a[7] = "msglen";
          sendError(user,a);
  			}
  		} else {
        char a[10] = "broadcast";
    		sendError(user,a);
  		}
  	} else {
      char a[10] = "broadcast";
  		sendError(user,a);
  	}
  } else {
  	char a[9] = "loggedin";
  	sendError(user,a);
  }
}

//Handles User Input
void * handle_user(void * arg){
	User user_;
	user_.fd = *(int *)arg;
	char buffer[MAX_BUFFER];
	int nc;
  char ** in = NULL;
  user_.name = (char*)calloc(20,sizeof(char));
  strcpy(user_.name," ");
	do {
    nc = recv(user_.fd,buffer,MAX_BUFFER,0);
		if(nc==-1){
			fprintf(stderr, "CHILD %ld: ERROR recv() failed\n", pthread_self());
      break;
		} else if (nc==0) {
      close(user_.fd);
      remove_client(&user_);
      break;
    } else {
      buffer[nc] = '\0';
      int argnum = Parse(buffer,&in,' ',1);
      if(strcmp(in[0], "LOGIN") == 0)
  			login(&user_,buffer,&in,argnum);
  		else if(strcmp(in[0], "LOGOUT\n") == 0)
  			logout(&user_,&in,argnum);
  		else if(strcmp(in[0], "SHARE") == 0)
  			share(&user_,buffer,argnum);
  		else if(strcmp(in[0], "SEND") == 0)
  			Send(&user_,buffer,argnum);
  		else if(strcmp(in[0], "BROADCAST") == 0)
  			broadcast(&user_,&in,argnum);
  		else if(strcmp(in[0], "WHO\n") == 0)
  			who(&user_);
  		else if(strcmp(in[0], "HELP") == 0)
  			help(&user_);
  		else {
  		send(user_.fd, "ERROR: Invalid command.\n", 24,0);
      printf("CHILD %ld: Sent ERROR: Invalid command.\n" , pthread_self());
    }
    }
	} while(nc>0);
	printf("CHILD %ld: Client disconnected\n", pthread_self());
  free(user_.name);
	pthread_exit(NULL);
}
//IPV6 SUPPORT
//IPV4 SUPPORT
void init(int port, int ipv) {
	//SOCKET/SERVER INFO
	int tcp, udp;
 	socklen_t serverlen, clientlen;
 	pthread_t tid;
  struct sockaddr_in server, client;
  struct sockaddr_in6 server6;
  if (ipv == 4) {

 	serverlen = sizeof(server);
  memset(&server, 0, serverlen);
  clientlen = sizeof(client);

    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    server.sin_family = PF_INET;
    if ((udp = socket(PF_INET, SOCK_DGRAM, 0))<0) {
      perror("ERROR: failed to create udp socket");
      exit(1);
    }
    if((tcp = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR: failed to create tcp socket");
        exit(1);
    }
    if(bind(tcp, (struct sockaddr *)&server, serverlen) < 0) {
        perror("ERROR: tcp bind failed");
        exit(1);
    }
    if (bind(udp, (struct sockaddr *)&server, serverlen)<0) {
      perror("ERROR: udp bind failed");
      exit(1);
    }
    if (listen(tcp, 32) < 0) {
        perror("ERROR: failed to listen");
        exit(1);
    }
  } else {
    //IPV6 SUPPORT, LEFTOVER FROM NETPROG 2018
    //while not necessary, i saw no point in removing it
    server6.sin6_addr = in6addr_any;
    server6.sin6_port = htons(port);
    server6.sin6_family = AF_INET6;
    serverlen = sizeof(server6);
    if((tcp = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("ERROR: failed to create tcp socket");
        exit(1);
    }
    const int on = 0;
    setsockopt(tcp, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
    if(bind(tcp, (struct sockaddr *)&server6, serverlen) < 0) {
        perror("ERROR: tcp failed to bind");
        exit(1);
    }
    if (listen(tcp, 32) < 0) {
        perror("ERROR: tcp failed to listen");
        exit(1);
    }
  }
    printf("MAIN: Listening for TCP connections on port: %d\n", port);
    printf("MAIN: Listening for UDP datagrams on port: %d\n", port);
    char buff[MAX_BUFFER];
    int rc,n;
    fd_set fds;
    while(1){
      FD_ZERO(&fds);
      FD_SET(udp,&fds);
      FD_SET(tcp, &fds);
      if (select(FD_SETSIZE,&fds, NULL,NULL,NULL)<0) {
        perror("MAIN: select failed");
        exit(1);
      }
      if (FD_ISSET(udp,&fds)) {
        n = recvfrom(udp, buff, 1024, 0, (struct sockaddr *)&client, (socklen_t *)&clientlen);
        printf("MAIN: Rcvd incoming UDP datagram from: %s\n", inet_ntoa(client.sin_addr));
        buff[n] = '\0';
        char ** input = NULL;
        int commands = Parse(buff, &input,' ',1);
        if (strcmp(input[0], "LOGIN")==0) {
          pthread_mutex_lock(&mutex);
          sendto(udp, "ERROR LOGIN not supported over UDP\n", 35, 0, (struct sockaddr *)&client, clientlen );
          pthread_mutex_unlock(&mutex);
          printf("MAIN: Sent ERROR LOGIN not supported over UDP\n");
        } else if (strcmp(input[0], "LOGOUT\n")==0) {
          pthread_mutex_lock(&mutex);
          sendto(udp, "ERROR LOGOUT not supported over UDP\n", 36, 0, (struct sockaddr *)&client, clientlen );
          pthread_mutex_unlock(&mutex);
          printf("MAIN: Sent ERROR LOGOUT not supported over UDP\n");
        } else if (strcmp(input[0], "WHO\n")==0) {
          //who
          printf("MAIN: Rcvd WHO request\n");
          sprintf(buff,"OK!\n");
          pthread_mutex_lock(&mutex);
          for (int i = 0; i < num_users; i++) {
            if (strcmp(users[i].name," ")!=0) {
              sprintf(buff+strlen(buff),"%s\n", users[i].name);
            } else {
              break;
            }
          }
          sendto(udp,buff,strlen(buff),0,(struct sockaddr *)&client, clientlen);
          pthread_mutex_unlock(&mutex);
        } else if (strcmp(input[0], "SEND")==0) {
          pthread_mutex_lock(&mutex);
          sendto(udp, "ERROR SEND not supported over UDP\n", 34, 0, (struct sockaddr *)&client, clientlen );
          pthread_mutex_unlock(&mutex);
          printf("MAIN: Sent ERROR SEND not supported over UDP\n");
        } else if (strcmp(input[0], "BROADCAST")==0) {
          printf("MAIN: Rcvd BROADCAST request\n");
            if(commands ==2) {
              char** args = NULL;
              int n = Parse(input[1],&args,'\n',1);
              if (n==2) {
                if (atoi(args[0])>=1 && atoi(args[0])<=990) {
                  sendto(udp, "OK!\n", 4, 0,(struct sockaddr *)&client, clientlen);
                  for (int i = 0; i < num_users; i++) {
                    if (strcmp(users[i].name," ")!=0) {
                      sprintf(buff, "FROM UDP-client %s %s", args[0],args[1]);
                      pthread_mutex_lock(&mutex);
                      send(users[i].fd, buff, strlen(buff), 0);
                      pthread_mutex_unlock(&mutex);
                    }
                  }//end for loop
                } else {
                  pthread_mutex_lock(&mutex);
                  sendto(udp,"ERROR Invalid msg length\n",25,0,(struct sockaddr *)&client, clientlen);
                  pthread_mutex_unlock(&mutex);
                  printf("MAIN: Sent ERROR Invalid msg length\n");
                }
              } else {
                pthread_mutex_lock( &mutex );
						    sendto(udp, "ERROR Invalid BROADCAST format\n", 31, 0, (struct sockaddr *)&client, clientlen );
						    pthread_mutex_unlock( &mutex );
						    printf("MAIN: Sent ERROR (Invalid BROADCAST format)\n");
              }
            } else {
              pthread_mutex_lock( &mutex );
              sendto(udp, "ERROR Invalid BROADCAST format\n", 31, 0, (struct sockaddr *)&client, clientlen );
              pthread_mutex_unlock( &mutex );
              printf("MAIN: Sent ERROR (Invalid BROADCAST format)\n");
            }
        } else if (strcmp(input[0], "SHARE")==0){
          pthread_mutex_lock(&mutex);
          sendto(udp, "ERROR SHARE not supported over UDP\n", 35, 0, (struct sockaddr *)&client, clientlen );
          pthread_mutex_unlock(&mutex);
          printf("MAIN: Sent ERROR SHARE not supported over UDP\n");
        } else {
          pthread_mutex_lock(&mutex);
          sendto(udp, "ERROR: unkown command given\n", 35, 0, (struct sockaddr *)&client, clientlen );
          pthread_mutex_unlock(&mutex);
          printf("MAIN: Sent ERROR: unknown command given\n");
        }
        continue; //skip tcp stuff
      }
      if (FD_ISSET(tcp, &fds)) {
        //tcp segment same as regular chat server
        printf("MAIN: Rcvd incoming TCP connection from: %s\n", inet_ntoa(client.sin_addr));
    	   int newsocket = accept(tcp, (struct sockaddr *)&client, (socklen_t *)&clientlen);
         users[num_users].fd = newsocket;
         users[num_users].name = (char*)calloc(20,sizeof(char));
         strcpy(users[num_users].name," ");
        // users[num_users].istcp = 1;
         num_users++;
    	    if((rc = pthread_create(&tid, NULL, handle_user,&newsocket) )!= 0){
    		      fprintf(stderr, "MAIN: Could not create thread %d\n", rc);
    		      exit(1);
    	   }
    }
  }
}

int main(int argc, char * argv[]){
  setvbuf(stdout, NULL, _IONBF, 0);
  int port;
  if (argc==2) {
    port = atoi(argv[1]);
  } else {
    perror("MAIN: ERROR USAGE: ./a.out <port>\n");
    exit(EXIT_FAILURE);
  }
  printf("MAIN: Started server\n");
  users = (User*)calloc(32, sizeof(User));
  //channels = (Channel*)calloc(10, sizeof(struct Channel));
  init(port,4);
	//Initiatizes Server Socket

	return 0;
}
