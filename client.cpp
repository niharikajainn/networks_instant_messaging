#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>

#include <string>
#include <iostream>
#include <map>

using namespace std;

int check_login(char* login_message, int ret, int loginTime, int loginSuccessTime, int sockfd_tx, struct sockaddr_in serv_addr);

int check_message(std::map<string, string> messages_sent, std::map<string, string>::iterator it, std::map<string, int> message_times, int ret, int sockfd_tx, struct sockaddr_in serv_addr);

FILE * fp;

int main()
{
    srand(time(NULL));
    
    int ret;
    int sockfd_tx = 0;
    int sockfd_rx = 0;
    char send_buffer[1024];
    char recv_buffer[1024];
    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    int maxfd;
    fd_set read_set;
    FD_ZERO(&read_set);
    
    sockfd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_tx < 0)
    {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }
    
    // The "serv_addr" is the server's address and port number,
    // i.e, the destination address if the client need to send something.
    // Note that this "serv_addr" must match with the address in the
    // UDP receive code.
    // We assume the server is also running on the same machine, and
    // hence, the IP address of the server is just "127.0.0.1".
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("192.168.0.12");
    serv_addr.sin_port = htons(32000);
    
    sockfd_rx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_rx < 0)
    {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }
    
    // The "my_addr" is the client's address and port number used for
    // receiving responses from the server.
    
    char hostbuffer[256];
    char *ipbuffer;
    struct hostent *host_entry;
    int hostname;
    
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    ipbuffer = inet_ntoa(*((struct in_addr*)
                           host_entry->h_addr_list[0]));
    
    int port_number = rand() % 1000 + 50000;
    
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(ipbuffer);
    my_addr.sin_port = htons(port_number);
    
    // Bind "my_addr" to the socket for receiving message from the server.
    bind(sockfd_rx,
         (struct sockaddr *) &my_addr,
         sizeof(my_addr));
    
    maxfd = sockfd_rx + 1; // Note that the file descriptor of stdin is "0"
    
    char token[6];
    
    int file_acknowledge = 0;
    int loginTime = 0;
    int loginSuccessTime = 0;
    char login_message[1024];
    
    std::map<string, string> messages_sent;
    std::map<string, string>::iterator it;
    
    std::map<string, int> message_times;
    
    
    while (1)
    {
        check_login(login_message, ret, loginTime, loginSuccessTime, sockfd_tx, serv_addr);
        check_message(messages_sent, it, message_times, ret, sockfd_tx, serv_addr);
        
        // Use select to wait on keyboard input or socket receiving.
        FD_SET(fileno(stdin), &read_set);
        FD_SET(sockfd_rx, &read_set);
        
        
        if(file_acknowledge)
        {
            file_acknowledge = 0;
            ret = recv(sockfd_rx, recv_buffer, sizeof(recv_buffer), 0);
            if(ret < 0)
            {
                printf("recvfrom() error: %s. \n", strerror(errno));
                return -1;
            }
        }
      
        select(maxfd, &read_set, NULL, NULL, NULL);
        
        
        if (FD_ISSET(fileno(stdin), &read_set))
        {
            
            //Now we know there is a keyboard input event
            
            fgets(send_buffer, sizeof(send_buffer), stdin);
            
            char send_buffer_copy[1024];
            strcpy(send_buffer_copy, send_buffer);
            string message_id;
            
            int event;
            if(string(send_buffer_copy).find("login") != std::string::npos)
            {
                event = 0; //login
            }
            else if(string(send_buffer_copy).find("logoff") != std::string::npos)
            {
                event = 1; //logoff
            }
            else if(string(send_buffer_copy).find("#file") != std::string::npos)
            {
                event = 2; //send a file to client_b
            }
            else
            {
                event = 3; //send a message to client_b
            }
            
            switch(event)
            {
                //login
                case 0:
                {
                    loginTime = time(NULL);
                    //client_a->server#login<password>
                    char* sender = strtok(send_buffer_copy, "->");
                    strtok(NULL, "<");
                    char* password = strtok(NULL, ">");
                    string append_addr = string(sender) + "->server#login<" + string(password) + "><" + ipbuffer + "><" + to_string(port_number) + ">";
                    strcpy(send_buffer, append_addr.c_str());
                    strcpy(login_message, send_buffer);
                    
                    break;
                }
                //logoff
                case 1:
                {
                    char* end = send_buffer_copy + strlen(send_buffer_copy) - 1;
                    while(end > send_buffer_copy && isspace((unsigned char)*end))
                    {
                        end--;
                    }
                    end[1] = '\0';
                    
                    strcat(send_buffer_copy, "<");
                    strcat(send_buffer_copy, token);
                    strcat(send_buffer_copy, ">");
                    strcpy(send_buffer, send_buffer_copy);
                    break;
                }
                //send a file to client_b
                case 2:
                {
                    
                    //client_a->clientb#file<filename>
                    char* sender = strtok(send_buffer_copy, "->");
                    char* receiver = strtok(NULL, ">#");
                    strtok(NULL, "<");
                    char* filename = strtok(NULL, ">");
                    string file = filename;
                    string receive = receiver;
                    
                    fp = fopen(filename, "rb");
                    fseek(fp, 0L, SEEK_END);
                    int filesize = ftell(fp);
                    rewind(fp);
                    string filesize_string = to_string(filesize);
                    
                    for(int i = 0; i < 10; i++)
                    {
                        message_id += rand() % 10 + 48;
                    }
                    
                    //client_a->clientb#<message_id>file<filename><filesize>
                    strcpy(send_buffer_copy, sender);
                    strcat(send_buffer_copy, "->");
                    strcat(send_buffer_copy, receive.c_str());
                    strcat(send_buffer_copy, "#<");
                    strcat(send_buffer_copy, message_id.c_str());
                    strcat(send_buffer_copy, ">file<");
                    strcat(send_buffer_copy, file.c_str());
                    strcat(send_buffer_copy, "><");
                    strcat(send_buffer_copy, filesize_string.c_str());
                    strcat(send_buffer_copy, ">");
                    
                    message_times[message_id] = time(NULL);
                    messages_sent[message_id] = send_buffer_copy;
                    
                    //send file info
                    ret = sendto(sockfd_tx,                   // the socket file descriptor
                                 send_buffer_copy,                    // the sending buffer
                                 sizeof(send_buffer_copy), // the number of bytes you want to send
                                 0,
                                 (struct sockaddr *) &serv_addr, // destination address
                                 sizeof(serv_addr));             // size of the address
                    if(ret <= 0) {
                        printf("send() error: %s.\n", strerror(errno));
                        break;
                    }
                    
                    strcpy(send_buffer_copy, "");
                    
                    //send file
                    int bytes_sent = 0;
                    while(bytes_sent+1024 < filesize)
                    {
                        fread(send_buffer_copy, 1, sizeof(send_buffer_copy), fp); //read and send first (filesize - filesize%1024) bytes of file
                        ret = sendto(sockfd_tx,                   // the socket file descriptor
                                     send_buffer_copy,                    // the sending buffer
                                     sizeof(send_buffer_copy), // the number of bytes you want to send
                                     0,
                                     (struct sockaddr *) &serv_addr, // destination address
                                     sizeof(serv_addr));             // size of the address
                        if(ret <= 0) {
                            printf("send() error: %s.\n", strerror(errno));
                            break;
                        }
                        bytes_sent += ret;
                    }
                    
                    fread(send_buffer_copy, 1, filesize - bytes_sent, fp); //read and send last filesize%1024 bytes of file
                    
                    strcpy(send_buffer, send_buffer_copy);
                    
                    ret = sendto(sockfd_tx,                   // the socket file descriptor
                                 send_buffer_copy,                    // the sending buffer
                                 sizeof(send_buffer_copy), // the number of bytes you want to send
                                 0,
                                 (struct sockaddr *) &serv_addr, // destination address
                                 sizeof(serv_addr));             // size of the address
                    if(ret <= 0) {
                        printf("send() error: %s.\n", strerror(errno));
                        break;
                    }
                    bytes_sent += ret;
                    fclose(fp); //every byte of the file was sent
                    break;
                }
                //send a message to client_b
                case 3:
                {
                    for(int i = 0; i < 10; i++)
                    {
                        message_id += rand() % 10 + 48;
                    }
                    
                    char* sender = strtok(send_buffer_copy, "->");
                    char* receiver = strtok(NULL, "#");
                    char* message = strtok(NULL, "\0");
                    
                    message_times[message_id] = time(NULL);
                    
                    strcpy(send_buffer, strtok(send_buffer, "#"));
                    strcat(send_buffer, "#<");
                    strcat(send_buffer, token);
                    strcat(send_buffer, "><");
                    strcat(send_buffer, message_id.c_str());
                    strcat(send_buffer, ">");
                    strcat(send_buffer, message);
                    
                    char* end = send_buffer + strlen(send_buffer) - 1;
                    while(end > send_buffer && isspace((unsigned char)*end))
                    {
                        end--;
                    }
                    end[1] = '\0';
                    
                    //messages sent and not acknowledged as received stored here
                    messages_sent[message_id] = send_buffer;
                    break;
                }
            }
            
        
            ret = sendto(sockfd_tx,                   // the socket file descriptor
                         send_buffer,                    // the sending buffer
                         sizeof(send_buffer), // the number of bytes you want to send
                         0,
                         (struct sockaddr *) &serv_addr, // destination address
                         sizeof(serv_addr));             // size of the address
            
            if (ret <= 0) {
                printf("sendto() error: %s.\n", strerror(errno));
                return -1;
            }
            
            
        }
        
        if (FD_ISSET(sockfd_rx, &read_set))
        {
            
            //Now we know there is an event from the network
            ret = recv(sockfd_rx, recv_buffer, sizeof(recv_buffer), 0);
            if(ret < 0)
            {
                printf("recvfrom() error: %s. \n", strerror(errno));
                return -1;
            }
            
            printf("%s\n", recv_buffer);
            
            char recv_buffer_copy[1024];
            strcpy(recv_buffer_copy, recv_buffer);
            
            //successful login or logout
            if(string(recv_buffer_copy).find("Success<") != std::string::npos)
            {
                loginSuccessTime = time(NULL);
                strtok(recv_buffer_copy, "<");
                strcpy(token, strtok(NULL, ">"));
            }
            //receive a file
            else if(string(recv_buffer_copy).find(">file") != std::string::npos)
            {
                //client_a->client_b#<msg_id>file<filename><filesize>
                char* sender = strtok(recv_buffer_copy, "->");
                char* receiver = strtok(NULL, ">#<");
                char* message_id = strtok(NULL, "<>");
                strtok(NULL, "<");
                char* filename = strtok(NULL, ">");
                char* filesize_string = strtok(NULL, "<>");
                int filesize = atoi(filesize_string);
                
                fp = fopen(filename, "wb");
                strcpy(recv_buffer, "");
                
                int bytes_received = 0;
                while(bytes_received + 1024  < filesize)
                {
                    ret = recv(sockfd_rx, recv_buffer, sizeof(recv_buffer), 0);
                    if(ret < 0)
                    {
                        printf("recvfrom() error: %s. \n", strerror(errno));
                        return -1;
                    }
                    
                    fwrite(recv_buffer, 1, sizeof(recv_buffer), fp); //receive and write first (filename - filename%1024 bytes of file)
                    bytes_received += ret;
                }
                
                //receive and write last filesize%1024 bytes of file
                ret = recv(sockfd_rx, recv_buffer, sizeof(recv_buffer), 0);
                if(ret < 0)
                {
                    printf("recvfrom() error: %s. \n", strerror(errno));
                    return -1;
                }
                
                fwrite(recv_buffer, 1, filesize - bytes_received, fp);
                fclose(fp); //all bytes received
                
                //receiver->server#<msg_id>File received
                string response = "";
                response += string(receiver) + "->server#<" + string(message_id) + ">File received";
                strcpy(send_buffer, response.c_str());
                
                ret = sendto(sockfd_tx,                   // the socket file descriptor
                             send_buffer,                    // the sending buffer
                             sizeof(send_buffer), // the number of bytes you want to send
                             0,
                             (struct sockaddr *) &serv_addr, // destination address
                             sizeof(serv_addr));             // size of the address
                
                if (ret <= 0) {
                    printf("sendto() error: %s.\n", strerror(errno));
                    return -1;
                }
                cout << send_buffer << endl;
                file_acknowledge = 1;
                
            }
            else
            {
                //forwarded message
                if(string(recv_buffer_copy).find("server") == std::string::npos)
                {
                    //client_a->client_b#<ABCXYZ><msg_id>some_message
                    char* sender = strtok(recv_buffer_copy, "->");
                    char* receiver = strtok(NULL, ">#<");
                    strtok(NULL, "<>");
                    char* msg_id = strtok(NULL, "<>");
                    char* msg = strtok(NULL, "\0");
                    
                    //send back
                    //client_b->server#<ABCXYZ><msg_id>Success: some_message
                    
                    string response = "";
                    response += string(receiver) + "->server#<" + string(token) + "><" + string(msg_id) + ">Success: " + string(msg);
                    
                    strcpy(send_buffer, response.c_str());
                    
                    ret = sendto(sockfd_tx,                   // the socket file descriptor
                                 send_buffer,                    // the sending buffer
                                 sizeof(send_buffer), // the number of bytes you want to send
                                 0,
                                 (struct sockaddr *) &serv_addr, // destination address
                                 sizeof(serv_addr));             // size of the address
                    
                    if (ret <= 0) {
                        printf("sendto() error: %s.\n", strerror(errno));
                        return -1;
                    }
                    
                }
                //acknowledgment of message/file sent
                else
                {
                    char* msg_id;
                    //message, sends back token
                    if(string(recv_buffer_copy).find("File") == std::string::npos)
                    {
                        strtok(recv_buffer_copy, "->");
                        strtok(NULL, ">#<");
                        strtok(NULL, "<>");
                        msg_id = strtok(NULL, "<>");
                    }
                    else
                    {
                        strtok(recv_buffer_copy, "->");
                        strtok(NULL, ">#<");
                        msg_id = strtok(NULL, "<>");
                    }
                    
                    //message received, erase from sent-and-not-acknowledged database
                    messages_sent.erase(msg_id);
                    message_times.erase(msg_id);
                }

            }
            
            
        }
        
        // Now we finished processing the pending event. Just go back to the
        // beginning of the loop and wait for another event.
        
    } // This is the end of the while loop
    
} // This is the end of main()

int check_login(char* login_message, int ret, int loginTime, int loginSuccessTime, int sockfd_tx, struct sockaddr_in serv_addr)
{
    if(loginTime && !loginSuccessTime && time(NULL)-loginTime > 5)
    {
        cout << "Resend login message" << endl;
        ret = sendto(sockfd_tx,                   // the socket file descriptor
                     login_message,                    // the sending buffer
                     sizeof(login_message), // the number of bytes you want to send
                     0,
                     (struct sockaddr *) &serv_addr, // destination address
                     sizeof(serv_addr));             // size of the address
        
        if (ret <= 0) {
            printf("sendto() error: %s.\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

int check_message(std::map<string, string> messages_sent, std::map<string, string>::iterator it, std::map<string, int> message_times, int ret, int sockfd_tx, struct sockaddr_in serv_addr)
{
    for(it = messages_sent.begin(); it != messages_sent.end(); it++)
    {
        if(time(NULL) - message_times[it->first] > 5)
        {
            //resend message
            if(messages_sent[it->first].find("File") == std::string::npos)
            {
                cout << "resend message with id " << it->first << endl;
                ret = sendto(sockfd_tx,                   // the socket file descriptor
                             messages_sent[it->first].c_str(),                    // the sending buffer
                             sizeof(messages_sent[it->first].c_str()), // the number of bytes you want to send
                             0,
                             (struct sockaddr *) &serv_addr, // destination address
                             sizeof(serv_addr));             // size of the address
                
                if (ret <= 0) {
                    printf("sendto() error: %s.\n", strerror(errno));
                    return -1;
                }
            }
            //resend file info and file
            else
            {
                //resend file info
                cout << "resend file with id " << it->first << endl;
                ret = sendto(sockfd_tx,                   // the socket file descriptor
                             messages_sent[it->first].c_str(),                    // the sending buffer
                             sizeof(messages_sent[it->first].c_str()), // the number of bytes you want to send
                             0,
                             (struct sockaddr *) &serv_addr, // destination address
                             sizeof(serv_addr));             // size of the address
                
                if (ret <= 0) {
                    printf("sendto() error: %s.\n", strerror(errno));
                    return -1;
                }
                
                char fileInfo[1024];
                strcpy(fileInfo, messages_sent[it->first].c_str());
                
                strtok(fileInfo, "->");
                strtok(NULL, ">#<");
                strtok(NULL, "<>");
                strtok(NULL, "<");
                char* filename = strtok(NULL, ">");
                char* filesize_string = strtok(NULL, "<>");
                int filesize = atoi(filesize_string);
                
                fp = fopen(filename, "rb");
                
                char send_buffer_copy[1024];
                
                //resend file
                int bytes_sent = 0;
                while(bytes_sent+1024 < filesize)
                {
                    fread(send_buffer_copy, 1, sizeof(send_buffer_copy), fp); //read and send first (filesize - filesize%1024) bytes of file
                    ret = sendto(sockfd_tx,                   // the socket file descriptor
                                 send_buffer_copy,                    // the sending buffer
                                 sizeof(send_buffer_copy), // the number of bytes you want to send
                                 0,
                                 (struct sockaddr *) &serv_addr, // destination address
                                 sizeof(serv_addr));             // size of the address
                    if(ret <= 0) {
                        printf("send() error: %s.\n", strerror(errno));
                        break;
                    }
                    bytes_sent += ret;
                }
                
                fread(send_buffer_copy, 1, filesize - bytes_sent, fp); //read and send last filesize%1024 bytes of file
                
                ret = sendto(sockfd_tx,                   // the socket file descriptor
                             send_buffer_copy,                    // the sending buffer
                             sizeof(send_buffer_copy), // the number of bytes you want to send
                             0,
                             (struct sockaddr *) &serv_addr, // destination address
                             sizeof(serv_addr));             // size of the address
                if(ret <= 0) {
                    printf("send() error: %s.\n", strerror(errno));
                    break;
                }
                bytes_sent += ret;
                fclose(fp); //every byte of the file was sent
            }
        }
    }
    return 0;
}
