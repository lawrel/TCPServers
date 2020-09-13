#ifndef chatHeader_h_
#define chatHeader_h_
#include <arpa/inet.h>
#include <sys/un.h>
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
#include <netdb.h>

pthread_mutex_t mutex;
#define MAX_BUFFER 1024
#define MAX_READ_LEN 512
#define MAX_FILENAME_LEN 100
#define MAX_PACKETS 99
#define MAX_TRIES 4
#define TIME_OUT 5 //5 seconds
#define MAX_USERS 10
#define SERVERPORT "4950"
//Struct USER: Defines what a user is
//FD -> File desciptor
//Name of USER
//Operator options (enabled/disabled)
struct User{
	int fd;
	char name[20];
	int oper;
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
typedef struct {
	struct sockaddr_in client;
	int sockno;
	socklen_t clilen;
}udp_user;

//abstract functions -> implemented later in code
void quit(struct User * user, const char * arg1, const char * arg2, const char * arg3);
void * udp_handler(void * arg);
int Parse(char * buf, int size, char * command, char * arg1, char * arg2, char * message);

char password[20]; //Password for operator if given
struct User * users; //Array of all users
int num_users; //Current number of Users
int max_users; //MAX_USERS
struct Channel * channels; //List of Channels
int num_channels; //Current number of Channels
int max_channels; // MAX_CHANNELS

typedef enum {
  RRQ = 1,
  WRQ = 2,
  DATA = 3,
  ACK = 4,
  ERR = 5,
  NONE = 6
} opcode;

typedef enum {
  UNDEFINED = 0,
  FILE_NOT_FOUND,
  ACCESS_VIOLATION,
  DISK_FULL,
  ILLEGAL_OPERATION,
  UNKNOWN_ID,
  FILE_ALREADY_EXISTS,
  NO_USER
} errcode;

//converts block number to length-2 string
void s_to_i(char *f, int n);
int tftp_handler();
void * get_addr(struct sockaddr *sa);
int checkTime(int sockID, char *buf, struct sockaddr_storage cliSock, socklen_t addr_len);
char* make_rrq(char *filename);
char* make_wrq(char *filename);
char* make_data(int block, const char *data);
char* make_ack(const char* block);
char* make_err(const char *errcode, const char* errmsg);
#endif
