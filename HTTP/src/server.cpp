/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

#define MAXSOCKETSIZE 1024

#define FAILURE_SIGNAL 1

using namespace std;

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

string charArrayToString(char* ca, ssize_t size) {
	string s = "";
	for (ssize_t i = 0; i < size; i++) {
		s += ca[i];
	}
	return s;
}

int sendd(string feedback, int sockfd) {
	feedback += "\r\n\r\n";
	return send(sockfd, feedback.c_str(), feedback.length(), 0);
	}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if (argc != 2) {
	    fprintf(stderr,"usage: server port\n");
	    exit(1);
	}


	string port = string(argv[1]);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

    

	if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			ssize_t bytes_read = 0;
            char buffer[MAXSOCKETSIZE];
            string request = "";
			string request_type = "";
			string directory = "";
			bool conti = true;
				bytes_read = recv(new_fd, buffer, MAXSOCKETSIZE, 0);
				if (bytes_read > 0) {
					request = string(buffer);
					request_type = charArrayToString(buffer,3);
					if (request_type != "GET") {
						string feedback = "HTTP/1.1 400 Bad Request\r\n";
					feedback += "\r\n";
					ssize_t index = send(sockfd, feedback.c_str(), feedback.length(), 0);
					if (index == -1) {
						perror("400 Bad-Request.");
					}
					}
					
					// get directory:
					ssize_t i = 0;

					while(i < request.length() && request[i] != '/') {
						i++;
					}
					
					int j = i;
					while(j < request.length() && request[j] != ' '){
						j++;
					}
			
					directory = request.substr(i+1,j-i-1);
					
				} else {
					conti = false;
				}
			
 			cout<< "|" << directory << "|"<<endl;
			char send_buffer[MAXSOCKETSIZE];
			FILE *file = fopen(directory.c_str(), "r+");
			if (file != NULL) {
				
				
				if (!feof(file)) {
					while (1) {
						string feedback = "HTTP/1.1 200 OK\r\n\r\n";
						size_t send_sig = fread(send_buffer, 1, MAXSOCKETSIZE - feedback.length(), file);
						if (send_sig <= 0) {
							break;
						} 
						string returnvalue = feedback+ string(send_buffer);
						send(new_fd, returnvalue.data(), returnvalue.size(), 0);
						if (feof(file)) {
							break;
						}
					}
				}
				fclose(file);
				printf("finished");
				
			} else {
				string feedback = "HTTP/1.1 404 Not Found";
				int send_signal = sendd(feedback, new_fd);
	  			if (send_signal == -1) {
					perror("404 FILE NOT FOUND");
				}
			}
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}
