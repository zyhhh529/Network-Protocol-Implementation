/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include <string>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 1000 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void parseUrl(const std::string &arg1, std::string &host, std::string &port, std::string &file_path) 
{
	int index = arg1.find_first_of("//");
	std::string url = arg1.substr(index+2);
	index = url.find('/');
	host = url.substr(0,index);
	file_path = url.substr(index);
	if(host.find(':') != host.npos) {
		index = host.find(':');
		port = host.substr(index+1);
		host = host.substr(0, index);
	} else {
		port = "80";
	}

}
using namespace std;

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	std::string host, port, file_path;
	parseUrl(argv[1], host, port, file_path);
	// cout<<host<<"  "<<port<<"  "<<file_path<<endl;
	// std::string request = "GET " + file_path + " HTTP/1.1\r\n" + "User-Agent: Wget/1.12(linux-gnu)\r\n" + "Host: " + host + ":" + port + "\r\n" + "Connection: Keep-Alive\r\n\r\n";
	// cout<<request<<endl;

	if ((rv = getaddrinfo(host.data(), port.data(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	// connect successfully, send request
	std::string request = "GET " + file_path + " HTTP/1.1\r\n" + "User-Agent: Wget/1.12(linux-gnu)\r\n" + "Host: " + host + ":" + port + "\r\n" + "Connection: Keep-Alive\r\n\r\n";
	send(sockfd, request.c_str(), request.size(), 0);

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	//     perror("recv");
	//     exit(1);
	// }

	// buf[numbytes] = '\0';

	// printf("client: received '%s'\n",buf);

	// process data and store to output
	FILE *out = fopen("output", "wb");
	int head_flag = 1;
	char* start;
	memset(buf, 0, MAXDATASIZE);

	while(1) {
		if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0){
			if(head_flag) {
				start = strstr(buf,"\r\n\r\n") + 4;
				head_flag = 0;
				fwrite(start, sizeof(char), strlen(start), out);
			} else {
				fwrite(buf, sizeof (char), numbytes, out);
			}
		} else {
			break;
		}

	}

	fclose(out);

	close(sockfd);

	return 0;
}
