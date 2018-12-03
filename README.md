# networks_instant_messaging
A command-line instant-messaging application using C/C++ socket programming on Linux, which allows clients to log in and reliably transfer messages and files to one another.


The application assumes the server IP address and port number are well-known, and hardcodes these in server.cpp and client.cpp. All possible client usernames and passwords are stored in passwords.txt, and will be used by the server to verify correct credentials.

A client with some (username) and (password) can login with keyboard input of the form "(username)->server#login<(password)>". Clients can send messages to one other with keyboard input of the form "(client_a_username)->(client_b_username)#(some message)". Clients can send files to one another with keyboard input of the form "(client_a_username)->(client_b_username)#file<(some filename)>".

Console output will include error messages, forwarded messages, and acknowledgments. Files transferred will be downloaded to the current working directory.
