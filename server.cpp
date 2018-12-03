#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>

using namespace std;

struct client_info
{
    string username;
    string password;
    int online;
    string ip = "";
    int port;
    string token = "";
    time_t last_sent;
};

std::map<string, struct client_info> clients;
std::map<string, struct client_info>::iterator client_iterator;

void printClient(struct client_info* client);

int main()
{
    ifstream myInputFile;
    myInputFile.open("passwords.txt");
    if(!myInputFile.is_open())
    {
        cout << "Error opening file." << endl;
        return 1;
    }
    
    std::map<string, string> acknowledgments; //message ID, ACK message
    
    while(!myInputFile.eof())
    {
        string name;
        struct client_info* client = new struct client_info;
        myInputFile >> name;
        client->username = name;
        myInputFile >> client->password;
        client->online = 0;
        
        clients[name] = *client;
    }
    
    myInputFile.close();
    
    FILE * fp;
    fp = fopen("file.txt", "w");
    fclose(fp);
    
    int ret;
    int sockfd;
    struct sockaddr_in servaddr, cliaddr, cliaddr_2;
    char recv_buffer[1024];
    int recv_len;
    socklen_t len;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }
    
    // The servaddr is the address and port number that the server will
    // keep receiving from.
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_addr.s_addr = inet_addr("192.168.0.12");
    //servaddr.sin_port = htons(57314);
    servaddr.sin_addr.s_addr = inet_addr("192.168.0.12");
    servaddr.sin_port = htons(32000);
 
    bind(sockfd,
         (struct sockaddr *) &servaddr,
         sizeof(servaddr));
    
    while (1)
    {
        for(client_iterator = clients.begin(); client_iterator != clients.end(); client_iterator++)
        {
            time_t now = time(NULL);
            if(clients[client_iterator->first].online)
            {
                if (now - clients[client_iterator->first].last_sent > 300)
                {
                    clients[client_iterator->first].online = 0;
                }
            }
        }
        
        len = sizeof(cliaddr);
        recv_len = recvfrom(sockfd, // socket file descriptor
                            recv_buffer,       // receive buffer
                            sizeof(recv_buffer),  // number of bytes to be received
                            0,
                            (struct sockaddr *) &cliaddr,  // client address
                            &len);             // length of client address structure
        
        if (recv_len <= 0) {
            printf("recvfrom() error: %s.\n", strerror(errno));
            return -1;
        }
        
        printf("%s\n", recv_buffer);
        
        char recv_buffer_copy[1024];
        strcpy(recv_buffer_copy, recv_buffer);
        char send_buffer[1024] = "server->";
        char message[1024];
        string username;
        
        int event;
        if(string(recv_buffer_copy).find("login") != std::string::npos)
        {
            event = 0; //login
        }
        else if(string(recv_buffer_copy).find("logoff") != std::string::npos)
        {
            event = 1; //logoff
        }
        else if(string(recv_buffer_copy).find(">file") != std::string::npos)
        {
            event = 2;
        }
        else
        {
            if(string(recv_buffer_copy).find("Success") != std::string::npos)
            {
                event = 3; //acknowledging message received
            }
            else
            {
                event = 4; //sending message to forward
            }
        }
        
        switch(event)
        {
            //login
            case 0:
            {
                //client_a->server#login<password><ip><port>
                char* client = strtok(recv_buffer_copy, "->");
                username = string(client);
                strtok(NULL, "<");
                char* password = strtok(NULL, "><");
                char* ip = strtok(NULL, "><");
                char* port = strtok(NULL, "><");
                
                clients[username].last_sent = time(NULL);
                clients[username].ip = string(ip);
                clients[username].port = stoi(port);
                
                //verify login
                int login_sent = (clients[username].password == string(password));
                strcat(send_buffer, client); //"server->client_a"
                
                if(login_sent)
                {
                    clients[username].online = 1;
                    strcat(send_buffer, "#Success<");
                    
                    char token[7] = "";
                    for(int i = 0; i < 6; i++)
                    {
                        int random = rand() % 94 + 33;
                        while(random == 35 || random == 60 || random == 62) //delimiters
                        {
                            random = rand() % 94 + 33;
                        }
                        token[i] = random;
                    }
                    token[6] = '\0';
                    
                    clients[username].token = string(token);
                    clients[username].port = stoi(port);
                    strcat(send_buffer, token);
                    strcat(send_buffer, ">");
                }
                else
                {
                    char error[1024] = "#Error: password does not match!";
                    strcat(send_buffer, error);
                    
                }
                break;
            }
            //logoff
            case 1:
            {
                //client_a->server#logoff<XYZABC>
                char* client = strtok(recv_buffer_copy, "->");
                username = string(client);
                clients[username].online = 0;
                
                //server->client_a#Success<XYZABC>
                strcat(send_buffer, client); //"server->client_a"
                strcat(send_buffer, "#Success<");
                strcat(send_buffer, clients[username].token.c_str());
                strcat(send_buffer, ">");
                
                break;
            }
            //file forward
            case 2:
            {
                //client_a->clientb#<message_id>file<filename><filesize>
                
                //forward file info to client_b
                strcpy(message, recv_buffer_copy);
                
                char* sender = strtok(recv_buffer_copy, "->");
                username = string(sender);
                char* receiver = strtok(NULL, ">#<");
                char* message_id = strtok(NULL, "<>");
                strtok(NULL, "<");
                char* filename = strtok(NULL, ">");
                char* filesize_string = strtok(NULL, "<>");
                int filesize = atoi(filesize_string);

                clients[username].last_sent = (NULL);
                
                memset(&cliaddr_2, 0, sizeof(cliaddr_2));
                cliaddr_2.sin_family = AF_INET;
                cliaddr_2.sin_addr.s_addr = inet_addr(clients[string(receiver)].ip.c_str());
                cliaddr_2.sin_port = htons(clients[string(receiver)].port);
                
                int sendto_status = sendto(sockfd, message, sizeof(message), 0, (struct sockaddr*) &cliaddr_2, sizeof(cliaddr_2));
                if(sendto_status < 0)
                {
                    printf("sendto() error.\n");
                    return -1;
                }
                
                strcpy(recv_buffer, "");
                
                //forward file to client_b
                int bytes_received = 0;
                int bytes_sent = 0;
                while(bytes_received < filesize)
                {
                    ret = recvfrom(sockfd, // socket file descriptor
                                   recv_buffer,       // receive buffer
                                   sizeof(recv_buffer),  // number of bytes to be received
                                   0,
                                   (struct sockaddr *) &cliaddr,  // client address
                                   &len);             // length of client address structure
                    if(ret < 0)
                    {
                        printf("recvfrom() error: %s. \n", strerror(errno));
                        return -1;
                    }
                    bytes_received += ret;
                    ret = sendto(sockfd,                   // the socket file descriptor
                                 recv_buffer,                    // the sending buffer
                                 sizeof(recv_buffer), // the number of bytes you want to send
                                 0,
                                 (struct sockaddr *) &cliaddr_2, // destination address
                                 sizeof(cliaddr_2));             // size of the address
                    if(ret <= 0) {
                        printf("send() error: %s.\n", strerror(errno));
                        break;
                    }
                    bytes_sent += ret;
                }
                
                //receive and send last filesize%1024 bytes of file
                ret = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
                if(ret < 0)
                {
                    printf("recvfrom() error: %s. \n", strerror(errno));
                    return -1;
                }
                bytes_received += ret;
                ret = sendto(sockfd,                   // the socket file descriptor
                             recv_buffer,                    // the sending buffer
                             sizeof(recv_buffer), // the number of bytes you want to send
                             0,
                             (struct sockaddr *) &cliaddr_2, // destination address
                             sizeof(cliaddr_2));             // size of the address
                if(ret <= 0) {
                    printf("send() error: %s.\n", strerror(errno));
                    break;
                }
                bytes_sent += ret;
                
                socklen_t len2 = sizeof(cliaddr_2);
                //wait for acknowledgment from client_b
                ret = recvfrom(sockfd, // socket file descriptor
                               recv_buffer,       // receive buffer
                               sizeof(recv_buffer),  // number of bytes to be received
                               0,
                               (struct sockaddr *) &cliaddr_2,  // client address
                               &len2);             // length of client address structure
                if(ret < 0)
                {
                    printf("recvfrom() error: %s. \n", strerror(errno));
                    return -1;
                }
                cout << recv_buffer << endl;
                
                strtok(recv_buffer, "#");
                strcpy(send_buffer, "");
                strcat(send_buffer, "server->");
                strcat(send_buffer, sender);
                strcat(send_buffer, "#");
                strcat(send_buffer, strtok(NULL, "\0"));
                
                //send acknowledgment to client_a
                
                break;
            }
            //acknowledge message received
            case 3:
            {
                //client_b->server#<ABCXYZ><msg_id>Success: some_message
                strtok(recv_buffer_copy, "->");
                strtok(NULL, ">#<");
                strtok(NULL, "<>");
                char* msg_id = strtok(NULL, "<>");
                
                strcpy(send_buffer, acknowledgments[msg_id].c_str());
                //server->client_a#<XYZABC><msg_id>Success: some_message
                
                char findClientA[1024];
                strcpy(findClientA, acknowledgments[msg_id].c_str());
                strtok(findClientA, "->");
                char* receiver = strtok(NULL, ">#<");
                username = string(receiver);
                break;
            }
            //message forward
            case 4:
            {
                //client_a->client_b#<XYZABC><msg_id>some_message
                char* sender = strtok(recv_buffer_copy, "->");
                username = string(sender);
                char* receiver = strtok(NULL, ">#<");
                char* token = strtok(NULL, "<>");
                char* msg_id = strtok(NULL, "<>");
                char* msg = strtok(NULL, "#");
                
                clients[username].last_sent = time(NULL);
                
                int token_match = 0;
                if(clients[username].token.compare(string(token)) == 0)
                {
                    token_match = 1;
                }
                
                strcat(send_buffer, username.c_str()); //"server->client_a#<XYZABC><msg_id"
                strcat(send_buffer, "#<");
                strcat(send_buffer, clients[string(sender)].token.c_str());
                strcat(send_buffer, "><");
                strcat(send_buffer, msg_id);
                
                if(!token_match)
                {
                    //>Error: token error!
                    strcat(send_buffer, ">Error: token error!");
                }
                else
                {
                    if(!clients[string(receiver)].online)
                    {
                        //>Error: destination offline!
                        strcat(send_buffer, ">Error: destination offline!");
                    }
                    else
                    {
                        //forward message and
                        //>Success: some_message
                        strcat(send_buffer, ">Success: ");
                        strcat(send_buffer, msg);
                        acknowledgments[msg_id] = send_buffer;
                        
                        string forward_message = "";
                        forward_message += string(sender) + "->" + string(receiver) + "#<" + clients[string(receiver)].token + "><" + string(msg_id) + ">" + string(msg);
                        
                        strcpy(message, forward_message.c_str());
                        
                        memset(&cliaddr_2, 0, sizeof(cliaddr_2));
                        cliaddr_2.sin_family = AF_INET;
                        cliaddr_2.sin_addr.s_addr = inet_addr(clients[string(receiver)].ip.c_str());
                        cliaddr_2.sin_port = htons(clients[string(receiver)].port);
                        
                        int sendto_status = sendto(sockfd, message, sizeof(message), 0, (struct sockaddr*) &cliaddr_2, sizeof(cliaddr_2));
                        if(sendto_status < 0)
                        {
                            printf("sendto() error.\n");
                            return -1;
                        }
                        continue;
                    }
                    
                }
                break;
            }
        } //end of event handler
        
        printf("%s\n", send_buffer);
        memset(&cliaddr, 0, sizeof(cliaddr));
        cliaddr.sin_family = AF_INET;
        cliaddr.sin_addr.s_addr = inet_addr(clients[username].ip.c_str());
        cliaddr.sin_port = htons(clients[username].port);
        
        int sendto_status = sendto(sockfd, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));
        
        if(sendto_status < 0)
        {
            printf("sendto() error.\n");
            return -1;
        }
            
    }//end of while

    
    
    return 0;
    
}

void printClient(struct client_info* client)
{
    cout << "Username: " << client->username << endl;
    cout << "Password: " << client->password << endl;
    cout << "Logged in: " << client->online << endl;
    cout << "Token: " << client->token << endl;
    cout << "IP: " << client->ip << endl;
    cout << "Port: " << client->port << endl;
}
