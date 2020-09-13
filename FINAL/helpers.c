#include "chatHeader.h"
void * get_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void s_to_i(char *f, int n) {
  if(n==0){
    f[0] = '0', f[1]='0', f[2]='\0';
  } else if (n%10 > 0 && n/10 == 0) {
    char c = n+'0';
    f[0] = '0', f[1]=c, f[2]='\0';
  } else if (n%100>0&&n/100==0) {
    char c2 = (n%10)+'0';
    char c1 = (n/10)+'0';
    f[0] = c1, f[1]=c2, f[2]='\0';
  } else {
    f[0] = '9', f[1]='9', f[2]='\0';
  }
}
char* make_rrq(char *filename){
  char *packet;
  packet = (char*)malloc(2+strlen(filename));
  bzero(packet,2+strlen(filename));
  strcat(packet, "01");
  strcat(packet,filename);
  return packet;
}
char* make_wrq(char *filename) {
  char *packet;
  packet = (char*)malloc(2+strlen(filename));
  bzero(packet,2+strlen(filename));
  strcat(packet, "02");
  strcat(packet,filename);
  return packet;
}
char* make_data(int block, const char *data) {
  char *packet;
  char temp[3];
  s_to_i(temp, block);
  packet = (char*)malloc(4+strlen(data));
  bzero(packet,4+strlen(data));
  strcat(packet, "03");
  strcat(packet, temp);
  strcat(packet, data);
  return packet;
}
char* make_ack(const char* block) {
  char *packet;
  packet = (char*)malloc(2+strlen(block));
  bzero(packet,2+strlen(block));
  strcat(packet, "04");
  strcat(packet, block);
  return packet;
}
char* make_err(const char *errcode, const char* errmsg) {
  char *packet;
  packet = (char*)malloc(4+strlen(errmsg));
  bzero(packet,4+strlen(errmsg));
  strcat(packet, "05");
  strcat(packet, errcode);
  strcat(packet, errmsg);
  return packet;
}
int checkTime(int sockID, char *buf, struct sockaddr_storage cliSock, socklen_t addr_len) {
  fd_set f;
  int n;
  struct timeval t;
  FD_ZERO(&f);
  FD_SET(sockID, &f);
  t.tv_sec = TIME_OUT;
  t.tv_usec = 0;

  n = select(sockID+1, &f, NULL, NULL, &t);
  	if (n == 0){
  		printf("timeout\n");
  		return -2;
  	} else if (n == -1){
  		printf("error\n");
  		return -1;
  	}

  return recvfrom(sockID, buf, MAX_BUFFER-1 , 0, (struct sockaddr *)&cliSock, &addr_len);
}
