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
#include <time.h>
#include <ctype.h>
#include <pthread.h>

//Struct USER: Defines what a user is 
//FD -> File desciptor
//Name of USER
//Operator options (enabled/disabled)
struct User{
	int fd;
	char name[20];
	int operator;
};

//Struct Channel: Defines a channel 
//Name of channel
//Array of users
//Max# of users
//Current number of users
struct Channel{
	char name[20];
	struct User * users;
	int maxUsers;
	int numUsers;
}; 

//abstract Quit function -> implemented later in code
void quit(struct User * user, char * arg1, char * arg2, char * arg3);

char password[20]; //Password for operator if given 
struct User * users; //Array of all users 
int num_users = 0; //Current number of Users
int max_users = 10; //MAX_USERS
struct Channel * channels; //List of Channels
int num_channels = 0; //Current number of Channels
int max_channels = 10; // MAX_CHANNELS


//Adds user to server
void push_user(struct User user, struct User ** users_, int * nusers, int * musers){
	if(*nusers == *musers){
		*users_ = realloc(*users_, (*musers+10) * sizeof(struct User));
		*musers += 10;
	}
	(*users_)[*nusers] = user;
	(*nusers)++;
}
//Adds channel to server
void push_channel(struct Channel channel){
	if(num_channels == max_channels){
		channels = realloc(channels, (max_channels+10) * sizeof(struct Channel));
		max_channels += 10;
	}
	channels[num_channels] = channel;
	num_channels++;
}

//Looks for Channel
struct Channel * findChannel(char * name){
	if(name == NULL) return NULL;
	struct Channel * channel = NULL;
	for(int i = 0; i < num_channels; i++){
		if(strcmp(name, channels[i].name) == 0){
			channel = &channels[i];
			break;
		}
	}
	return channel;
}
//Remove USER 
int pop_user(char * name, struct User ** users_, int * nusers){
	int found = 0;
	for(int i = 0; i < *nusers; i++){
		if(strcmp(name, (*users_)[i].name) == 0){
			found = 1;
			(*nusers)--;
			if(i == *nusers) break;
		}
		if(found)
			(*users_)[i] = (*users_)[i+1];
	}
	return found;
}
//Looks for a USER 
struct User * get_user(char * name, struct User * users_, int size){
	if(name == NULL) return NULL;
	struct User * user = NULL;
	for(int i = 0; i < size; i++){
		if(strcmp(name, users_[i].name) == 0){
			user = &users_[i];
			break;
		}
	}
	return user;
}
//Read USER input
int Read(struct User * user, char * buffer, size_t n){
	int nc;
	read:
	if((nc = read(user->fd, buffer, n)) == -1){
		if(errno == EINTR){
			goto read;
		}
		else {
			quit(user, "", "", "");
		}
	}
	return nc;
}
//Sends user message
void Send(struct User * user, char * buffer, size_t length){
	if(send(user->fd, buffer, length, 0) != length) 
		quit(user, "", "", "");
}
//Parses given input from USER
int Parse(char * buf, int size, char * command, char * arg1, char * arg2, char * message){
	if(buf[size-1] != '\0'){
		buf[size] = '\0';
		size--;
	}
	int i, j, k;
	command[0] = '\0';
	arg1[0] = '\0';
	arg2[0] = '\0';
	message[0] = '\0';

	if(size == 0) return -1;

    i = strlen(buf)-1;
    while(i >= 0 && isspace(buf[i]))
        i--;
    buf[i+1] = '\0';
    size = i+1;

	for(i = 0; i <= size; i++){
		if(isspace(buf[i]) || buf[i] == '\0'){
			buf[i] = '\0';
			if(strlen(buf) > 20)
				return -1;
			strcpy(command, buf);
			break;
		}
	}
	i++;
	for(j = i; j <= size; j++){
		if(isspace(buf[j]) || buf[j] == '\0'){
			buf[j] = '\0';
			if(strlen(&buf[i]) > 20)
				return -1;
			strcpy(arg1, &buf[i]);
			break;
		}
	}
	j++;
	if(strcmp("PRIVMSG", command) == 0){
		k = j;
		if(strlen(&buf[k]) > 512)
			return -1;
		strcpy(message, &buf[k]);
		if(strlen(message)+k < size)
			return -1;
	} else {
		for(k = j; k <= size; k++){
			if(isspace(buf[k]) || buf[k] == '\0'){
				buf[k] = '\0';
				if(strlen(&buf[j]) > 20)
					return -1;
				strcpy(arg2, &buf[j]);
				break;
			}
		}
		k++;
	}
	return 0;
}
//Checks for proper formatting of Username/Password
int goodForm(char * string, int hashtag){
	int i = 0;
	if(strlen(string) < 1) return 0;
	if(hashtag){
		if(strlen(string) < 2) return 0;
		if(string[i] != '#')
			return 0;
		i++;
	}
	if(!isalpha(string[i])) return 0;
	i++;
	for(; i < strlen(string); i++)
		if(!(isalnum(string[i]) || string[i] == '_'))
			return 0;
	return 1;
}
//Allows USER to establish their username
void user(struct User * user, char * nickname, char * arg2, char * arg3){
	if(goodForm(nickname, 0) && strlen(arg2)+strlen(arg3) == 0 && get_user(nickname, users, num_users) == NULL){
		strcpy(user->name, nickname);
		push_user(*user, &users, &num_users, &max_users);

		char message[30];
		sprintf(message, "Welcome, %s\n", nickname);
		Send(user, message, strlen(message));
	} else {
		send(user->fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user->fd);
        pthread_exit(NULL);
	}	
}
//Lists all available channels 
void list(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[255];
	struct Channel * ch = findChannel(channel);
	if(ch == NULL){
		sprintf(msg, "There are currently %d channels.\n", num_channels);
		for(int i = 0; i < num_channels; i++){
			if(strlen(msg) > 230){
				Send(user, msg, strlen(msg));
				msg[0] = '\0';
			}
			strcat(msg, "* ");
			strcat(msg, &channels[i].name[1]);
			strcat(msg, "\n");
		}
	} else {
		sprintf(msg, "There are currently %d members.", ch->numUsers);
		if(ch->numUsers > 0) {
			strcat(msg, "\n");
			strcat(msg, channel);
			strcat(msg, " members:");
		}
		for(int i = 0; i < ch->numUsers; i++){
			if(strlen(msg) > 230){ 
				Send(user, msg, strlen(msg));
				msg[0] = '\0';
			}
			strcat(msg, " ");
			strcat(msg, (ch->users)[i].name);
		}
		strcat(msg, "\n");
	}
	Send(user, msg, strlen(msg));
}
//Allows USER to join a Channel 
void join(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	struct Channel * ch = findChannel(channel);
	if(ch == NULL){
		if(!goodForm(channel, 1)){
			Send(user, "Invalid command.\n", 17);
			return;
		}
		struct Channel newChannel;
		strcpy(newChannel.name, channel);
		newChannel.users = calloc(5, sizeof(struct User));
		newChannel.maxUsers = 5;
		newChannel.numUsers = 0;
		push_channel(newChannel);
		ch = &channels[num_channels-1];
	}

	if(get_user(user->name, ch->users, ch->numUsers)) return;
	char msg[100];
	sprintf(msg, "Joined channel %s\n", channel);
	Send(user, msg, strlen(msg));

	sprintf(msg, "%s> %s joined the channel\n", channel, user->name);
	for(int i = 0; i < ch->numUsers; i++)
		Send(&(ch->users[i]), msg, strlen(msg));

	push_user(*user, &(ch->users), &(ch->numUsers), &(ch->maxUsers));
}
//Allows USER to leave a Channel
void part(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[100];
	struct Channel * ch = findChannel(channel);
	if(strlen(channel) == 0){
		for(int i = 0; i < num_channels; i++){
			if(pop_user(user->name, &(channels[i].users), &(channels[i].numUsers))){
				sprintf(msg, "%s> %s left the channel.\n", channels[i].name, user->name);
				for(int j = 0; j < channels[i].numUsers; j++)
					Send(&(channels[i].users[j]), msg, strlen(msg));
				Send(user, msg, strlen(msg));
			}
		}
	} else if(ch == NULL || get_user(user->name, ch->users, ch->numUsers) == NULL){
		sprintf(msg, "You are not currently in %s\n", channel);
		Send(user, msg, strlen(msg));
	} else {
		sprintf(msg, "%s> %s left the channel.\n", ch->name, user->name);
		for(int i = 0; i < ch->numUsers; i++)
			Send(&(ch->users[i]), msg, strlen(msg));
		pop_user(user->name, &(ch->users), &(ch->numUsers));
	}
}
//Get's OPERATOR status 
//Only if password is correct
void operator(struct User * user, char * pass, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	if(strlen(password) == 0){
		Send(user, "OPERATOR status not available.\n", 31);
		return;
	}
	if(strcmp(password, pass) == 0){
		user->operator = 1;
		Send(user, "OPERATOR status bestowed.\n", 26);
	}
	else{
		Send(user, "Invalid OPERATOR command.\n", 26);
	}
}
//Kicks a user from Channel
//Only if Caller has OPERATOR status
void kick(struct User * user, char * channel, char * name, char * arg3){
	if(strlen(arg3) != 0 || strlen(channel) == 0 || strlen(name) == 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	if(!user->operator){
		Send(user, "You must have OPERATOR status.\n", 31);
		return;
	}

	char msg[100];
	struct Channel * ch = findChannel(channel);
	struct User * kick_user = get_user(name, users, num_users);
	if(kick_user == NULL){
		sprintf(msg, "\"%s\" is not on the server.\n", name);
		Send(user, msg, strlen(msg));
		return;
	}
	if(ch == NULL || get_user(name, ch->users, ch->numUsers) == NULL){
		sprintf(msg, "%s is not currently in %s\n", name, channel);
		Send(user, msg, strlen(msg));
		return;
	}

	sprintf(msg, "%s> %s has been kicked from the server.\n", ch->name, kick_user->name);
	for(int i = 0; i < ch->numUsers; i++)
		Send(&(ch->users[i]), msg, strlen(msg));
	pop_user(name, &(ch->users), &(ch->numUsers));
}

//Implements PRIVMSG 
//Locates user/channel to contact and sends a message
void privmsg(struct User * user, char * dest, char * arg2, char * message){
	if(strlen(arg2) > 0 || strlen(dest) == 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[550];
	if(dest[0] == '#'){
		struct Channel * ch = findChannel(dest);
		if(ch == NULL || get_user(user->name, ch->users, ch->numUsers) == NULL){
			sprintf(msg, "You are not currently in %s\n", dest);
			Send(user, msg, strlen(msg));
		} else {
			sprintf(msg, "%s> %s: %s\n", ch->name, user->name, message);
			for(int i = 0; i < ch->numUsers; i++)
				Send(&(ch->users[i]), msg, strlen(msg));
		}
	} else {
		struct User * dest_user = get_user(dest, users, num_users);
		if(dest_user == NULL){
			sprintf(msg, "\"%s\" is not on the server.\n", dest);
			Send(user, msg, strlen(msg));
		} else {
			sprintf(msg, "%s> %s\n", user->name, message);
			Send(dest_user, msg, strlen(msg));
		}
	}
}	

//Quits for one user 
void quit(struct User * user, char * arg1, char * arg2, char * arg3){
	if(strlen(arg1)+strlen(arg2)+strlen(arg3) > 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	pop_user(user->name, &users, &num_users);
	close(user->fd);
	part(user, "", "", "");
	pthread_exit(NULL);
}
void help(struct User * user) {
	char msg[256];
	sprintf(msg,"AVAILABLE COMMANDS:\n -OPERATOR <password>\n -LIST <channelname>\n -JOIN <#channel>\n -PART <#channel>\n -KICK <#channel> <user>\n -HELP \n -PRIVMSG <#channel|user> <message>\n -QUIT \n");
	Send(user,msg, strlen(msg));
}	

//Handles User Input
void * handle_user(void * arg){
	struct User user_;
	user_.operator = 0;
	user_.fd = *(int *)arg;
	char buffer[600];
	char command[20];
	char arg1[20];
	char arg2[20];
	char msg[256];
	char message[512];
	int nc;

	try_read:
	
	sprintf(msg,"To begin, enter a username using USER <name>\n");
	send(user_.fd,msg, strlen(msg),0);
	if((nc = read(user_.fd, buffer, 600)) == -1){
		if(errno == EINTR) {
			goto try_read;
		} else {
			close(user_.fd);
			pthread_exit(NULL);
		}
	}
	if(Parse(buffer, nc, command, arg1, arg2, message)){
		send(user_.fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user_.fd);
        pthread_exit(NULL);
	}
	if(strcmp(command, "USER") != 0){
		send(user_.fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user_.fd);
        pthread_exit(NULL);
	}
	user(&user_, arg1, arg2, message);

	while(1){
		nc = Read(&user_, buffer, 600);
		if(Parse(buffer, nc, command, arg1, arg2, message)){
			Send(&user_, "Invalid command.\n", 17);
            continue;
		}

		if(strcmp(command, "LIST") == 0)
			list(&user_, arg1, arg2, message);
		else if(strcmp(command, "JOIN") == 0)
			join(&user_, arg1, arg2, message);
		else if(strcmp(command, "PART") == 0)
			part(&user_, arg1, arg2, message);
		else if(strcmp(command, "OPERATOR") == 0)
			operator(&user_, arg1, arg2, message);
		else if(strcmp(command, "KICK") == 0)
			kick(&user_, arg1, arg2, message);
		else if(strcmp(command, "PRIVMSG") == 0)
			privmsg(&user_, arg1, arg2, message);
		else if(strcmp(command, "QUIT") == 0)
			quit(&user_, arg1, arg2, message);
		else if(strcmp(command, "HELP") == 0)
			help(&user_);
		else 
			Send(&user_, "Invalid command.\n", 17);
	}

	pthread_exit(NULL);
}
void init6() {
	//SOCKET/SERVER INFO
	int server_socket, new_socket;
 	socklen_t sockaddr_len;
 	int port;
 	pthread_t tid;
 	struct sockaddr_in6 sock_info;
 	sockaddr_len = sizeof(sock_info);
    memset(&sock_info, 0, sockaddr_len);

    srand(time(NULL));
    port = (rand() % 64511) + 1024;
    sock_info.sin6_addr = in6addr_any;
    sock_info.sin6_port = htons(port);
    sock_info.sin6_family = AF_INET6;

    if((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("failed to create socket");
        exit(1);
    }
    const int on = 0;
    setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("failed to bind");
        exit(1);
    }
    if (listen(server_socket, 2) < 0) {  
        perror("failed to listen");  
        exit(1);  
    } 
    printf("Server is active on Port %d\n", port);
    printf("Server is running IPV6\n");

    while(1){
    	if((new_socket = accept(server_socket, (struct sockaddr *)&sock_info, (socklen_t *)&sockaddr_len)) < 0){
    		if(errno == EINTR) continue;
    		perror("accept");
    		exit(1);
    	}
    	if(pthread_create(&tid, NULL, handle_user, (void *)&new_socket) != 0){
    		fprintf(stderr, "ERROR: Could not create thread\n");
    		exit(1);
    	}
    }
}
void init4() {
	//SOCKET/SERVER INFO
	int server_socket, new_socket;
 	socklen_t sockaddr_len;
 	int port;
 	pthread_t tid;
 	struct sockaddr_in sock_info;
 	sockaddr_len = sizeof(sock_info);
    memset(&sock_info, 0, sockaddr_len);

    srand(time(NULL));
    port = (rand() % 64511) + 1024;
    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(port);
    sock_info.sin_family = AF_INET;

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("failed to create socket");
        exit(1);
    }
    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("failed to bind");
        exit(1);
    }
    if (listen(server_socket, 2) < 0) {  
        perror("failed to listen");  
        exit(1);  
    } 
    printf("Server is active on Port %d\n", port);
    printf("Server is running IPV4\n");
    while(1){
    	if((new_socket = accept(server_socket, (struct sockaddr *)&sock_info, (socklen_t *)&sockaddr_len)) < 0){
    		if(errno == EINTR) continue;
    		perror("accept");
    		exit(1);
    	}
    	if(pthread_create(&tid, NULL, handle_user, (void *)&new_socket) != 0){
    		fprintf(stderr, "ERROR: Could not create thread\n");
    		exit(1);
    	}
    }
}

//Main code
int main(int argc, char * argv[]){

    users = calloc(10, sizeof(struct User));
    channels = calloc(10, sizeof(struct Channel));
	
    if(argc == 1)
    	password[0] = '\0';
    if(argc == 2){
    	if(strncmp(argv[1], "--opt-pass=", 11) != 0 && strcmp(argv[1],"6")!=0 &&strcmp(argv[1],"4")!=0){
    		fprintf(stderr, "USAGE: ./hw3.out --opt-pass=[PASSWORD(optional)]\n");
    		return 1;
    	} if (strcmp(argv[1],"6")==0) {
    		init6();
    	} if (strcmp(argv[1],"4")==0) {
    		init4();
    	} if(goodForm(&(argv[1][11]), 0)){
    		strcpy(password, &(argv[1][11]));
    	} else {
    		fprintf(stderr, "Password was in an improper format. Password format:[a-zA-Z][_0-9a-zA-Z]* \n");
    		return 1;
    	}
    } 
    if (argc == 3) {
    	if(strncmp(argv[1], "--opt-pass=", 11) != 0 && strcmp(argv[1],"6")!=0 &&strcmp(argv[1],"4")!=0){
    		fprintf(stderr, "USAGE: ./hw3.out --opt-pass=[PASSWORD(optional)]\n");
    		return 1;
    	} if(goodForm(&(argv[1][11]), 0)){
    		strcpy(password, &(argv[1][11]));
    	} else {
    		fprintf(stderr, "Password was in an improper format. Password format:[a-zA-Z][_0-9a-zA-Z]* \n");
    		return 1;
    	}
    	if (strcmp(argv[2],"6")==0) {
    		init6();
    	} if (strcmp(argv[2],"4")==0) {
    		init4();
    	}
    } else if(argc > 3){
    	fprintf(stderr, "USAGE: ./hw3.out --opt-pass=[PASSWORD(optional)] [4/6 (optional)]\n");
    	return 1;
    } else {
    	init4();
    }
	//Initiatizes Server Socket
   	

	return 0;
}