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
#include <algorithm>
#include <cctype>
#include <random>

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

std::string generate_random_grade()
{
	static const std::vector<std::string> grades = {"A","B","C","D","F"};

	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<size_t> dist(0, grades.size() - 1);

	return grades[dist(gen)];
}

std::vector<Course> remove_duplicate_courses(const std::vector<Course>& courses)
{
	std::vector<Course> unique_courses;
	std::vector<std::string> seen_codes;

	for (const Course& c : courses)
	{
		std::string code_upper = c.course_code;
		std::transform(code_upper.begin(), code_upper.end(), code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
		
		bool alr_seen = false;
		for (const std::string& seen : seen_codes)
		{
			if (seen == code_upper)
			{
				alr_seen = true;
				break;
			}
		}

		if (!alr_seen)
		{
			seen_codes.push_back(code_upper);
			unique_courses.push_back(c);
		}
		else
		{
			std::cout << "Duplicate course code: " << c.course_code << ". Skipped\n";
		}
	}
	return unique_courses;
}

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
					"\n200 SUCCESS\n Avaiable commands are:\n"
					"\tCATALOG: access course catalog\n"
					"\tENROLLMENT: enroll or drop course\n"
					"\tMYCOURSES: manage schedules\n"
					"\tBYE: close and exit\n");
			break;
		case Mode::kCatalog:
			send_reply(client_fd,
					"\n200 SUCCESS\n CATALOG commands are:\n"
					"\tLIST [subject|instructor|course-code]: shows all available courses\n"
					"\tSEARCH <subject|instructor|course-code> <search-term>: finds courses within the filter and search-term\n"
					"\tSHOW <course-code> [availability]: displays details for course\n"
					"\n(aruguments within <> are required, [] are optional)\n");
			break;
		case Mode::kEnrollment:
			send_reply(client_fd,
					"\n200 SUCCESS\n ENROLLMENT commands are:\n"
					"\tENROLL <course-code>: enrolls you in a course\n"
					"\tDROP <course-code>: drops a specified course\n"
					"\n(arguments in <> are required)\n");
			break;
		case Mode::kMyCourses:
			send_reply(client_fd, 
					"\n200 SUCCESS\n MYCOURSES commands are:\n"
					"\tLIST: displays current enrollment\n"
					"\tVIEWGRADES: displays grades for completed courses\n");
			break;
	}
}

bool try_switch_mode(int client_fd, const std::string& command, const std::string& args, Mode& mode)
{
	if (command == "CATALOG")
	{
		if (!args.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: CATALOG does not take multiple arguments\n");
			return true;
		}
		else
		{
			mode = Mode::kCatalog;
			send_reply(client_fd, "210 SUCCESS: now entering CATALOG mode\n");
			return true;
		}
	}				
	if (command == "ENROLLMENT")
	{
		if (!args.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: ENROLLMENT does not take multiple arguments\n");
			return true;
		}
		else
		{
			mode = Mode::kEnrollment;
			send_reply(client_fd, "220 SUCCESS: now entering ENROLLMENT mode\n");
			return true;
		}
	}
	if (command == "MYCOURSES")
	{
		if (!args.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: MYCOURSES does not take multiple arguments\n");
			return true;
		}
		else
		{
			mode = Mode::kMyCourses;
			send_reply(client_fd, "230 SUCCESS: now entering MYCOURSES mode\n");
			return true;
		}
	}
	return false;
}

void handle_catalog_command(int client_fd, const std::string& command, const std::string& args, ServerState& state)
{
	std::istringstream iss(args);

	if (command == "LIST") 
	{
		std::string filter;
		iss >> filter;
		std::string filter_upper = filter;
		std::transform(filter_upper.begin(), filter_upper.end(), filter_upper.begin(), [](unsigned char c) { return std::toupper(c); }); 
		if (filter.empty())
		{
			std::string reply = "250 SUCCESS:\n";
			for (const Course& c : state.courses)
			{
				reply += c.course_code + ": " + c.title + " - " + c.subject + " - " + c.instructor + "\n";  
			}
			send_reply(client_fd, reply);
		}	
		else if (filter_upper == "SUBJECT")
		{
			std::string reply = "250 SUCCESS:\n";
			for (const Course& c : state.courses)
			{
				reply += c.subject + "\n";  
			}
			send_reply(client_fd, reply);
		}
		else if (filter_upper == "INSTRUCTOR")
		{
			std::string reply = "250 SUCCESS:\n";
			for (const Course& c : state.courses)
			{
				reply +=  c.instructor + "\n";  
			}
			send_reply(client_fd, reply);
		}
		else if (filter_upper == "COURSE-CODE")
		{
			std::string reply = "250 SUCCESS:\n";
			for (const Course& c : state.courses)
			{
				reply += c.course_code + "\n";  
			}
			send_reply(client_fd, reply);
		}
		else if (!filter_upper.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: '" + filter + "' is not recognized. Try SUBJECT, INSTRUCTOR, or COURSE-CODE\n");
			return;
		}
		else 
		{
			send_reply(client_fd, "304 NO CONTENT\n");
			return;
		}

	}
	else if (command == "SEARCH") //vectorCourse search_courses
	{
		std::string filter, search_term;
		iss >> filter >> search_term;
		if (filter.empty() || search_term.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: <filter> and <search-term> required\n");
			return;
		}
		std::lock_guard<std::mutex> lock(state.mutex);
		std::vector<Course> results = search_courses(state.courses, filter, search_term); 

		if (!results.empty())
		{
			std::string reply = "250 SUCCESS:\n";
			for (const Course& c : results)
			{
				reply += c.course_code + ": " + c.title + " - " + c.subject + " - " + c.instructor + "\n";  
			}
			send_reply(client_fd, reply);
		}
		else
		{
			send_reply(client_fd, "304 NO CONTENT: nothing matching filter or search term\n");
			return;
		}
	}
	else if (command == "SHOW") //Course get_course_by_code
	{
		std::string course_code, availability;
		iss >> course_code >> availability;
		std::string availability_upper = availability;
		std::transform(availability_upper.begin(), availability_upper.end(), availability_upper.begin(), [](unsigned char c) { return std::toupper(c); }); 

		std::lock_guard<std::mutex> lock(state.mutex);
		Course c = get_course_by_code(state.courses, course_code);

		if (c.course_code.empty())
		{
			send_reply(client_fd, "404 NOT FOUND: there are no matching courses\n");
			return;
		}
		else if (availability_upper == "AVAILABILITY")
		{
			std::string status = (c.seats_available > 0) ? "Open" : "Full";
			send_reply(client_fd, "250: SUCCESS\n\nAvailability: " + c.course_code + ": " + c.title + " - " + status + ", Seats: " + std::to_string(c.seats_available) + "\n\n");
		}
		else if (!availability.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: '" + availability + "' is not a valid argument. Try 'availability'\n");
			return;
		}
		else
		{
			send_reply(client_fd, "250: SUCCESS\n\n" + c.course_code + ": " + c.title + "\n" + "Instructor: " + c.instructor + "\n" + "Description: " + c.description + "\n\n");
		}
	}
	else
	{
		send_reply(client_fd, "400 BAD REQUEST: command not avaialble in CATALOG mode\n");
		return;
	}

}

void handle_enrollment_command(int client_fd, const std::string& command, const std::string& args, const std::string& username, ServerState& state)
{
	std::istringstream iss(args);

	if (command == "ENROLL") //bool enroll_in_course
	{
		std::string course_code;
		iss >> course_code;
		if (course_code.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: <course-code> is a required argument\n");
			return;
		}
		
		std::lock_guard<std::mutex> lock(state.mutex);
		Course c = get_course_by_code(state.courses, course_code);

		if (c.course_code.empty())
		{
			send_reply(client_fd, "404 NOT FOUND: there are no matching courses\n");
		}
		else
		{
			std::string target_upper = course_code;
			std::transform(target_upper.begin(), target_upper.end(), target_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });

			bool alr_enrolled = false;
			for (const GradeRecord& gr : state.student_records[username])
			{
				std::string gr_code_upper = gr.course_code;
				std::transform(gr_code_upper.begin(), gr_code_upper.end(), gr_code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
				if (gr_code_upper == target_upper)
				{
					alr_enrolled = true;
					break;
				}
			}
			if (alr_enrolled)
			{
				send_reply(client_fd, "403 FORBIDDEN: already enrolled in course\n");
				return;
			}

			std::vector<Course> enrolled_courses;
			for (const GradeRecord& gr : state.student_records[username])
			{
				enrolled_courses.push_back(get_course_by_code(state.courses, gr.course_code));
			}
			if (!check_prerequisites(enrolled_courses, c))
			{
				send_reply(client_fd, "403 FORBIDDEN: prereqs not met\n");
				return;
			}
			if (!enroll_in_course(state.courses, course_code))
			{
				send_reply(client_fd, "403 FORBIDDEN: course is full\n");
				return;
			}
			std::string grade = generate_random_grade();
			state.student_records[username].push_back(GradeRecord{course_code, grade});
			send_reply(client_fd, "250 ENROLLMENT SUCCESSFUL\nGrade assigned: " + grade + "\n");
		}
	}
	else if (command == "DROP") //bool drop_course
	{
		std::string course_code;
		iss >> course_code;
		if (course_code.empty())
		{
			send_reply(client_fd, "400 BAD REQUEST: <course-code> is a required argument\n");
			return;
		}
		
		std::lock_guard<std::mutex> lock(state.mutex);
		Course c = get_course_by_code(state.courses, course_code);

		if (c.course_code.empty())
		{
			send_reply(client_fd, "404 NOT FOUND: there are no matching courses\n");
			return;
		}
		else
		{
			std::string target_upper = course_code;
			std::transform(target_upper.begin(), target_upper.end(), target_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });

			bool is_enrolled = false;
			for (const GradeRecord& gr : state.student_records[username])
			{
				std::string gr_code_upper = gr.course_code;
				std::transform(gr_code_upper.begin(), gr_code_upper.end(), gr_code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
				if (gr_code_upper == target_upper)
				{
					is_enrolled = true;
					break;
				}
			}
			if (!is_enrolled)
			{
				send_reply(client_fd, "404 NOT FOUND: cannot drop course unless enrolled\n");
				return;
			}
			else if (drop_course(state.courses, course_code))
			{
				auto& records = state.student_records[username];
				for (size_t i = 0; i < records.size(); i++)
				{
					std::string rec_code_upper = records[i].course_code;
					std::transform(rec_code_upper.begin(), rec_code_upper.end(), rec_code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
			
					if (rec_code_upper == target_upper)
					{
						records.erase(records.begin() + i);
						break;
					}
				}
				
				std::string reply = "250 SUCCESS: " + course_code + " dropped\n";
				std::vector<std::string> just_dropped = {course_code};

				bool dropped_course;
				do 
				{
					dropped_course = false;
					for (size_t i = 0; i < records.size(); i++)
					{
						Course enrolled_course = get_course_by_code(state.courses, records[i].course_code);
						for (const std::string& required : enrolled_course.prerequisites)
						{
							std::string required_upper = required;
							std::transform(required_upper.begin(), required_upper.end(), required_upper.begin(),[](unsigned char ch) { return std::toupper(ch); });
							bool was_dropped = false;
							for (const std::string& d : just_dropped)
							{
								std::string d_upper = d;
								std::transform(d_upper.begin(), d_upper.end(), d_upper.begin(),[](unsigned char ch) { return std::toupper(ch); });
								if (d_upper == required_upper) { was_dropped = true; break; }
							}
							if (was_dropped)
							{
								drop_course(state.courses, records[i].course_code);
								reply += "250: prereqs no longer satisfied: " + records[i].course_code + "\n";
								just_dropped.push_back(records[i].course_code);
								records.erase(records.begin() + i);
								dropped_course = true;
								break;
							}
						}
						if (dropped_course) break;
					}
				} while (dropped_course);
				send_reply(client_fd, reply);
			}
			else
			{
				send_reply(client_fd, "500 SERVER ERROR\n");
			}
		}
	}
	else
	{
		send_reply(client_fd, "400 BAD REQUEST: command not available in ENROLLMENT mode\n");
	}
}

void handle_mycourses_command(int client_fd, const std::string& command, const std::string& args, const std::string& username, ServerState& state)
{
	std::istringstream iss(args);

	if (command == "LIST")
	{
		std::lock_guard<std::mutex> lock(state.mutex);
		if (state.student_records[username].empty())
		{
			send_reply(client_fd, "304 NO CONTENT\n");
		}
		else
		{
			std::string reply = "250 Enrollment History:\n";
			for (const GradeRecord& gr : state.student_records[username])
			{
				reply += " " + gr.course_code + "\n";
			}
			send_reply(client_fd, reply);
		}
	}
	else if (command == "VIEWGRADES") //bool get_student_grades
	{
		std::lock_guard<std::mutex> lock(state.mutex);
		if (state.student_records[username].empty())
		{
			send_reply(client_fd, "304 NO CONTENT\n");
		}
		else
		{
			std::string reply = "250 SUCCESS: Grades:\n";
			for (const GradeRecord& gr : state.student_records[username])
			{
				reply += " " + gr.course_code + ": " + gr.grade + "\n";
			}
			send_reply(client_fd, reply);
		}
	}
	else 
	{
		send_reply(client_fd, "400 BAD REQUEST: command not available in MYCOURSES mode\n");
	}
}
// Function to handle a single client connection in its own thread
void handle_client(int client_fd, struct sockaddr_storage their_addr, ServerState& state) 
{
	// A temporary buffer for the client's IP address string
	char s[INET6_ADDRSTRLEN];
	inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
	std::cout << "server: got connection from " << s << std::endl;

    // Implement the protocol interaction here.  
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

			std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) { return std::toupper(c); }); 

			size_t start = args.find_first_not_of(' ');
			args = (start == std::string::npos) ? "" : args.substr(start);
			while (!args.empty() && (args.back() == '\r' || args.back() == '\n'))
			{
				args.pop_back();
			}
			
			if (!identified) 
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
			else 
			{
				if (command == "BYE")
				{
					if (!args.empty())
					{
						send_reply(client_fd, "400 BAD REQUEST: BYE does not take multiple arguments\n");
					}
					else
					{
						send_reply(client_fd, "200 SUCCESS\nSee ya!\n");
					}
					break;
				}
				else if (command == "HELP")
				{
					if (!args.empty())
					{
						send_reply(client_fd, "400 BAD REQUEST: BYE does not take multiple arguments\n");
					}
					else 
					{
						send_help(client_fd, mode);
					}
				}
				else if (try_switch_mode(client_fd, command, args, mode))
				{
				}
				else
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
    state.courses = remove_duplicate_courses(load_courses_from_db(db_file_to_use));

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
    std::vector<std::jthread> threads;

    while (true) {
        sin_size = sizeof their_addr;
        client_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);

        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        // Create a new thread to handle the accepted connection
        // std::jthread automatically joins upon destruction
	threads.push_back(std::jthread(handle_client, client_fd, their_addr, std::ref(state)));
    }

    close(listen_fd);
    return 0;
}
