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
#include <sstream>
#include <mutex>
#include <unordered_map>

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

struct ServerState
{
	std::vector<Course> courses;
	std::unordered_map<std::string, std::vector<GradeRecord>> student_records;
	std::mutex mutex;
};

// Helper function to get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

enum class Mode {kNone, kCatalog, kEnrollment, kMyCourses};

void send_reply(int client_fd, const std::string& reply)
{
	send(client_fd, reply.c_str(), reply.size(), 0);
}

bool handle_iam(int client_fd, const std::string& args, const std::string& client_ip, std::string& username)
{
	username = args;
	
	if (username.empty())
	{
		send_reply(client_fd, "400 BAD REQUEST: no username submitted\n");
		return false;
	}
	send_reply(client_fd, "200 SUCCESS: Welcome " + username + "@" + client_ip + "\n");
	return true;

}

void send_help(int client_fd, Mode mode)
{
	switch (mode)
	{
		case Mode::kNone:
			send_reply(client_fd, 
					"200 SUCCESS\n Avaiable commands are:\n"
					"CATALOG: access course catalog\n"
					"ENROLLMENT: enroll or drop course\n"
					"MYCOURSES: manage schedules\n"
					"BYE: close and exit\n");
			break;
		case Mode::kCatalog:
			send_reply(client_fd,
					"200 SUCCESS\n CATALOG commands are:\n"
					"LIST [filter]: shows all available courses\n"
					"SEARCH <filter> <search-term>: finds courses within the filter and search-term\n"
					"SHOW <course-code> [availability]: displays details for course\n"
					"\n(aruguments within <> are required, [] are optional)\n");
			break;
		case Mode::kEnrollment:
			send_reply(client_fd,
					"200 SUCCESS\n ENROLLMENT commands are:\n"
					"ENROLL <course_code>: enrolls you in a course\n"
					"DROP <course_code>: drops a specified course\n"
					"\n(arguemnts in <> are required)\n");
			break;
		case Mode::kMyCourses:
			send_reply(client_fd, 
					"200 SUCCESS\n MYCOURSES commands are:\n"
					"LIST: displays current enrollment\n"
					"VIEWGRADES: displays grades for completed courses\n");
			break;
	}
}

bool try_switch_mode(int client_fd, const std::string& command, Mode& mode)
{
	if (command == "CATALOG")
	{
		mode = Mode::kCatalog;
		send_reply(client_fd, "210 SUCCESS: now entering CATALOG mode\n");
		return true;
	}				
	if (command == "ENROLLMENT")
	{
		mode = Mode::kEnrollment;
		send_reply(client_fd, "220 SUCCESS: now entering ENROLLMENT mode\n");
		return true;
	}
	if (command == "MYCOURSES")
	{
		mode = Mode::kMyCourses;
		send_reply(client_fd, "230 SUCCESS: now entering MYCOURSES mode\n");
		return true;
	}
	return false;
}

void handle_catalog_command(int client_fd, const std::string& command, const std::string& args, ServerState& state)
{
	std::istringstream iss(args);

	if (command == "LIST")
	{
	}
	else if (command == "SEARCH")
	{
	}
	else if (command == "SHOW")
	{
	}
	else
	{
		send_reply(client_fd, "400 BAD REQUEST: command not avaialble in CATALOG mode\n");
	}

}

void handle_enrollment_command(int client_fd, const std::string& command, const std::string& args, const std::string& username, ServerState& state)
{

}

void handle_mycourses_command(int client_fd, const std::string& command, const std::string& args, const std::string& username, ServerState& state)
{

}
// Function to handle a single client connection in its own thread
void handle_client(int client_fd, struct sockaddr_storage their_addr, ServerState& state) 
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
	Mode mode = Mode::kNone;
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
			
			std::istringstream iss(message);
			std::string command, args;
			iss >> command;
			std::getline(iss, args);

			size_t start = args.find_first_not_of(' ');
			args = (start == std::string::npos) ? "" : args.substr(start);
			while (!args.empty() && (args.back() == '\r' || args.back() == '\n'))
			{
				args.pop_back();
			}

			if (!identified) // 3. Ensure the proper mode sequencing (the client must send IAM first).
			{
				if (command == "IAM")
				{
					identified = handle_iam(client_fd, args, s, username);
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
					send_reply(client_fd, "200 SUCCESS\nSee ya!\n");
					break;
				}
				else if (command == "HELP")
				{
					send_help(client_fd, mode);
				}
				else if (try_switch_mode(client_fd, command, mode))
				{
				}
				else// 2. Parse the client's commands (e.g., IAM, CATALOG, ENROLLMENT, BYE).
 				{
					switch (mode)
					{
						case Mode::kNone:
							send_reply(client_fd, "400 BAD REQUEST: must enter mode first\n");
							break;
						case Mode::kCatalog:
							  handle_catalog_command(client_fd, command, args, state);
							  break;
						case Mode::kEnrollment:
							  handle_enrollment_command(client_fd, command, args, username, state);
							  break;
						case Mode::kMyCourses:
							  handle_mycourses_command(client_fd, command, args, username, state);
							  break;
					}
				}
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
    ServerState state;

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

    // 	Call your helper function to load the courses database here.
    // 	e.g., load_courses_from_db(db_file_to_use);
    // 	Do this BEFORE setting up the sockets and entering the main loop.
    state.courses = load_courses_from_db(db_file_to_use);

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
	std::vector<std::jthread> threads;
	threads.push_back(std::jthread(handle_client, client_fd, their_addr, std::ref(state)));
    }

    close(listen_fd);
    return 0;
}
