/*
 * CS447 P1 SERVER STARTER CODE
 * ----------------------------
 *  Author: Thoshitha Gamage
 *  Date:   09/16/2026
 *  Licence: MIT Licence
 *  Description: This is the starter code for CS447 Fall 2026 P1 server.
 *      This code is based on the simple stream server code found on 
 *      Beej's Guide to Network programming at https://beej.us/guide/bgnet/html/#a-simple-stream-server.
 *      The code was adapted to use C++20 features like std::jthread for concurrency.
 *
 *      This code can be compiled using:
 *           g++ -std=c++20 -Wall -pthread server.cpp p1_helper.cpp -o server
 *
 *      Use this code as the base for your server implementation.
 */

#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <system_error>
#include <fstream> 

// C headers for socket API
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "p1_helper.h"

#define BACKLOG 10

// Helper function to get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Function to handle a single client connection in its own thread
void handle_client(int client_fd, struct sockaddr_storage their_addr) 
{
	// A temporary buffer for the client's IP address string
	char s[INET6_ADDRSTRLEN];
	inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
	std::cout << "server: got connection from " << s << std::endl;

    // Implement the protocol interaction here.
    // 2. Parse the client's commands (e.g., IAM, CATALOG, ENROLLMENT, BYE).
    // 4. Send the appropriate reply codes (e.g., 200, 210, 403, 404, etc.) back to the client.
    
    	// 1. You must continuously recv() data from the client.  
    	char buf[1024];
	bool identified = false;
	bool catalog = false;
	bool enrollment = false;
	bool mycourses = false;
	std::string username;

	while (true)
	{
		int numbytes = recv(client_fd, buf, sizeof(buf) - 1, 0);

		if (numbytes == -1)
		{
			perror("recv");
			break;
		}
		else if (numbytes > 0)
		{
			buf[numbytes] = '\0'; 
			std::string message(buf);
			size_t space_pos = message.find_first_of(" \r\n");
			std::string command = message.substr(0, space_pos);

			if (!identified) // 3. Ensure the proper mode sequencing (the client must send IAM first).
			{
				if (command == "IAM")
				{
					if (space_pos != std::string::npos)
					{
						size_t start = message.find_first_not_of(" ", space_pos);
						size_t end = message.find_first_of("\r\n", start);
						if (start != std::string::npos)
						{
							username = message.substr(start, end - start);
						}
					}
					if (username.empty())
					{
						std::string reply = "400 BAD REQUEST: no username submitted\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
					else 
					{
						identified = true;
						std::string reply = "200 SUCCESS: Welcome " + username + "@" + s + "\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
				}
				else
				{
					std::string reply = "400 BAD REQUEST: must enter 'IAM' mode\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
				}
			}
			else // 2. Parse the client's commands (e.g., IAM, CATALOG, ENROLLMENT, BYE).
			{
				if (command == "BYE")
				{
					std::string reply = "200 SUCCESS\nBye bye\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
					break;
				}
    				if (command == "HELP")
				{
					if (!catalog && !enrollment && !mycourses)
					{
						std::string reply = "200 SUCCESS\n Avaiable commands are:\n"
							"CATALOG: access course catalog\n"
							"ENROLLMENT: enroll or drop course\n"
							"MYCOURSES: manage schedules\n"
							"BYE: close and exit\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
					else if (catalog)
					{
						std::string reply = "200 SUCCESS\n CATALOG commands are:\n"
							"LIST [filter]: shows all available courses\n"
							"SEARCH <filter> <search-term>: finds courses within the filter and search-term\n"
							"SHOW <course-code> [availability]: displays details for course\n"
							"\n(aruguments within <> are required, [] are optional)\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
					else if (enrollment)
					{
						std::string reply = "200 SUCCESS\n ENROLLMENT commands are:\n"
							"ENROLL <course_code>: enrolls you in a course\n"
							"DROP <course_code>: drops a specified course\n"
							"\n(arguemnts in <> are required)\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
					else if (mycourses)
					{
						std::string reply = "200 SUCCESS\n MYCOURSES commands are:\n"
							"LIST: displays current enrollment\n"
							"VIEWGRADES: displays grades for completed courses\n";
						send(client_fd, reply.c_str(), reply.size(), 0);
					}
				}
				if (command == "CATALOG")
				{	
					catalog = true;
					enrollment = false;
					mycourses = false;

					std::string reply = "200 SUCCESS: now entering CATALOG mode\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
				}
				if (command == "ENROLLMENT")
				{
					catalog = false;
					enrollment = true;
					mycourses = false;

					std::string reply = "200 SUCCESS: now entering ENROLLMENT mode\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
				}
				if (command == "MYCOURSES")
				{
					catalog = false;
					enrollment = false;
					mycourses = true;

					std::string reply = "200 SUCCESS: now entering MYCOURSES mode\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
				}
				/*
				else
				{
					std::string reply = "400 BAD REQUEST: unknown command\n";
					send(client_fd, reply.c_str(), reply.size(), 0);
				}
				*/
			}
		}
		else
		{
			std::cout << "connection closed." << std::endl;
			break;
		}
	}
    // Close the socket for this connection when the interaction is finished or BYE is received
	close(client_fd);
    	std::cout << "server: connection with " << s << " closed." << std::endl;
}

int main(int argc, char *argv[]) {
    int listen_fd, client_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes = 1;
    int rv;

    // Check if the server.conf file argument was provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server.conf>" << std::endl;
        return 1;
    }

    // Placeholders for configuration values. 
    // Students MUST populate these by parsing the conf file.
    std::string port_to_use = "";
    std::string db_file_to_use = "";

// Parse the 'server.conf' file (passed as argv[1]) to extract:
    std::ifstream conf_file(argv[1]);
    if (!conf_file.is_open())
    {
	    std::cerr << "Error: config file could not open." << std::endl;
	    return 1;
    }

    std::string line;
    while(std::getline(conf_file, line))
    {
	if(line.empty()) continue;

	size_t eq_pos = line.find('=');
	if (eq_pos == std::string::npos) continue;

	std::string key = line.substr(0, eq_pos);
	std::string value = line.substr(eq_pos + 1);

// 1. The PORT number and assign it to port_to_use
	if (key == "PORT")
	{
		port_to_use = value;
	}
// 2. The DB_FILE name and assign it to db_file_to_use
	else if (key == "DB_FILE")
	{
		db_file_to_use = value;
	}
    }

    if (port_to_use.empty() || db_file_to_use.empty()) {
        std::cerr << "Error: server.conf parsing not implemented. Variables are empty." << std::endl;
        return 1;
    }

    conf_file.close();

    // TODO: Call your helper function to load the courses database here.
    // e.g., load_courses_from_db(db_file_to_use);
    // Do this BEFORE setting up the sockets and entering the main loop.

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // use my IP

    // We use port_to_use.c_str() to convert the C++ string back to a C-style string for the socket API
    if ((rv = getaddrinfo(NULL, port_to_use.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(listen_fd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    std::cout << "server: waiting for connections on port " << port_to_use << "..." << std::endl;

    while (true) {
        sin_size = sizeof their_addr;
        client_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);

        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        // Create a new thread to handle the accepted connection
        // std::jthread automatically joins upon destruction
        std::jthread(handle_client, client_fd, their_addr);
    }

    close(listen_fd);
    return 0;
}
