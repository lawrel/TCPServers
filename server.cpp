#include <iostream>
#include <fstream>
#include <csignal>
#include <string>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <utility>
#include <vector>
#include <algorithm>
using namespace std;
#define MAXCLIENT 5

//GLOBALS TO STORE SOCKET DATA
int sockfd, newsockfd, maxfd, flag;
vector<pair<int,int>> cli_soc;
vector<string> cli_nms, dictionary;
set<string> usernames;
fd_set readfds;
struct sockaddr_in serverAddr,clientAddr;
string ans;
void startServ(int port,char* filename);
void ClientHandler();
void Read(int sock);
void CloseSock(int sock);
pair<int,int> Compare(string guess);
void CreateDict(string file);

//Initializes the server and begins listening
void startServ(int port,char* filename){
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    bzero(&(serverAddr),sizeof(serverAddr));
    int optval = 1;
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(int));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(port);

    bind(sockfd,(struct sockaddr*)&(serverAddr),sizeof(serverAddr));
    listen(sockfd,MAXCLIENT);
    socklen_t serverAddrLen = sizeof(serverAddr);
    getsockname(sockfd,(struct sockaddr*) &(serverAddr),&serverAddrLen);
    printf("Game started on port %d\n",ntohs(serverAddr.sin_port));

    //READ IN DICTIONARY AND SELECT A WORD
    CreateDict(filename);
    srand(time(0));
    ans = dictionary[(rand() % dictionary.size())];
    printf("The secret word is: %s\n",ans.c_str()); 

    //BEGIN GAME
    ClientHandler();
}
//Closes existing Socket (disconnects Player/Client)
void CloseSock(int socket){
    printf("%s left the game\n",cli_nms[socket].c_str());
    usernames.erase(cli_nms[socket]); //frees tge username
    close(cli_soc[socket].first);
    cli_soc.erase(cli_soc.begin()+socket);
    cli_nms.erase(cli_nms.begin()+socket);
}
pair<int,int> Compare(string guess){
    int placed = 0; int letter = 0;
    vector<char> v;

    if(guess=="quit") {
        return make_pair(-1,0);
    } else if(guess.length() != ans.length()){
        return make_pair(-1,-1);
    }

    for(int i = 0;i<guess.length();i++){
        v.push_back(guess[i]);
        if(tolower(guess[i]) == tolower(ans[i])){
            placed++;
        }
        for(int k = 0;k<ans.length();k++){
            if(tolower(v[i]) == tolower(ans[k])){
                v[i] = '.';
                letter++;
            }
        }
    }

    return make_pair(letter,placed);
}

void CreateDict(string file){
    ifstream wordfile;
    try{
        wordfile.open(file);
        string wordline;
        while(true){
            getline(wordfile,wordline);
            if(wordline.size()<=0){
                break;
            }
            dictionary.push_back(wordline);
        }
    }
    catch(int e){
        printf("%s could not be opened\n",file.c_str());
    }
}

//Handles Incoming Client Connections
void ClientHandler(){
    while(1) {
        FD_ZERO(&(readfds));
        FD_SET(sockfd, &(readfds));
        maxfd = sockfd;

        for(pair<int,int> i: cli_soc){ //add fds to read set
            if(i.first > 0){
                FD_SET(i.first, &readfds);
            }
            if(i.first > maxfd){
                maxfd = i.first;
            }
        }
        flag = select(maxfd+1,&readfds,NULL,NULL,NULL);
        if(flag < 0 && errno!=EINTR){
            printf("error: could not connect to client\n");
        }
        if(FD_ISSET(sockfd, &readfds)){
            socklen_t clientAddrSize = sizeof(clientAddr);
            int newfds = accept(sockfd,(struct sockaddr*)&clientAddr,&clientAddrSize);
            if(newfds<0){
                printf("Error: could not connect to client\n");
                continue;
            }
            printf("A client has connected on port %d\n",clientAddr.sin_port);

            //save this socket to our list of clients
            if(cli_soc.size() < MAXCLIENT){
                cli_soc.push_back(make_pair(newfds,1));
                cli_nms.push_back("");
                string user= "Please enter a username: \n";
                send(newfds,user.c_str(),sizeof(char)*user.length(),0);
            } else{
                printf("Too many clients are connected, connection rejected.\n");
            }
        }

        for(int i=0;i<cli_soc.size();i++){

            if(FD_ISSET(cli_soc[i].first, &readfds)){
                Read(i);
            }

        }

    }
}
//Handles Client inputs
void Read(int socket){
    string msg;
    char buff[1024];
    bzero(buff,sizeof(buff));
    int msgsize = read(cli_soc[socket].first,buff,1024);
    if(msgsize == 0){
        CloseSock(socket);
    }
    msg = string(buff);
    //strip newline from end
    for(int i = 0;i<msg.length();i++){
        if(msg[i] == '\n' || msg[i]==' '){
            msg.erase(msg.begin()+i);
        }
    }
    if(cli_soc[socket].second == 1){//1 is get username state
        printf("Client chose username: %s\n",msg.c_str());
        //check for duplicate names
        if(usernames.find(msg) == usernames.end()){//if username does not exist
            cli_nms[socket] = msg;
            usernames.insert(msg);
            cli_soc[socket] = make_pair(cli_soc[socket].first,2); //move from username to game
            //send users in game
            string servmsg = "Currently "+to_string(cli_soc.size()-1)+" others are playing."+
            "\nSecret word length is: "+to_string(ans.length())+"\n"+
            "Guess the secret word, or enter quit to leave: ";
            send(cli_soc[socket].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
        }
        else{
            //resend the username message
            printf("User requested existing name, %s\n",msg.c_str());
            string servmsg = "Username already in use, try a different name!\n";
            send(cli_soc[socket].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
        }
    }
    else if(cli_soc[socket].second == 2){
        //msg will be a word
        pair<int,int> compared = Compare(msg);
        if(compared.first==-1 && compared.second ==0) {
            shutdown(cli_soc[socket].first,SHUT_RD);
            CloseSock(socket);

        } else  if(compared.first == -1 && compared.second == -1){
            int dictionaryize = ans.length();
            string servmsg = "Your word was an incorrect size, correct length is: "+to_string(dictionaryize)+"\n\n";
            send(cli_soc[socket].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
            servmsg = "Guess the secret word, or enter quit to leave: ";
            send(cli_soc[socket].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
        }
        else if(compared.first == ans.length() && compared.first==compared.second){
            //send message to all cli_soc
            for(int i = 0; i < cli_soc.size(); i++){
                string servmsg = "\n"+cli_nms[socket]+" has guessed the word: "+ans+"\n";
                send(cli_soc[i].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
                servmsg = "New word will be selected; enter quit if you do not wish to play again.\n";
                send(cli_soc[i].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
            }
            //GET NEXT WORD
            srand(time(0));
            ans = dictionary[(rand() % dictionary.size())];
            printf("The secret word is: %s\n",ans.c_str());
            for(int i = 0; i < cli_soc.size(); i++){
                string servmsg = "The new secret word's length is: "+to_string(ans.length())+"\n";
                send(cli_soc[i].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
            }
            
        }
        else{
            for(int i = 0; i < cli_soc.size(); i++){
                string servmsg = "\n"+cli_nms[socket]+" guessed "+msg+": "+to_string(compared.first)+
                " letters(s) were correct\n\t\t\t"+to_string(compared.second)+" letters correctly placed.\nGuess the secret word, or enter quit to leave: ";
                send(cli_soc[i].first,servmsg.c_str(),sizeof(char)*servmsg.length(),0);
            }
        }
    }
}
//Main
int main(int argc, char* argv[]){
	//Handles arguments 
    if(argc == 3){ //If given portno
       startServ(atoi(argv[2]),argv[1]);
    }
    else if (argc==2){//If no portno
        startServ(0,argv[1]);
    } else {//If incorrect arguments
        printf("USAGE: ./server.out [DICTIONARY] [PORT NO. (optional)]\n");
    } 
    
}
