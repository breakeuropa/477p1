/*
 * P1 HELPER
 * ---------
 * Author: Thoshitha Gamage
 * Date: 09/16/2026
 * License: MIT License
 * Description: This helper function loads the courses from a .db file into a vector of Course structs.
 *              This code is intended to be used as a helper function in CS447 Fall 2026 P1 server code.
 */
#include "p1_helper.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

/**
 * @brief Loads course data from the specified file into memory.
 * @param filename The name of the database file (e.g., "courses.db").
 * @return A vector of Course structs containing all loaded courses.
 */
std::vector<Course> load_courses_from_db(const std::string& filename) {
    std::vector<Course> courses;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open database file " << filename << std::endl;
        return courses;
    }

    std::string line;
    std::getline(file, line); // Skip the header line

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string segment;
        Course course;

        // Course Code
        std::getline(ss, segment, ';');
        course.course_code = segment;

        // Title
        std::getline(ss, segment, ';');
        course.title = segment;

        // Subject
        std::getline(ss, segment, ';');
        course.subject = segment;

        // Instructor
        std::getline(ss, segment, ';');
        course.instructor = segment;

        // Prerequisites (comma-separated list)
        std::getline(ss, segment, ';');
        if (!segment.empty()) {
            std::stringstream prereq_ss(segment);
            std::string prereq_code;
            while (std::getline(prereq_ss, prereq_code, ',')) {
                course.prerequisites.push_back(prereq_code);
            }
        }

        // Seats Available
        std::getline(ss, segment, ';');
        try {
            course.seats_available = std::stoi(segment);
        } catch (...) {
            course.seats_available = 0;
        }

        // Capacity
        std::getline(ss, segment, ';');
        try {
            course.capacity = std::stoi(segment);
        } catch (...) {
            course.capacity = 0;
        }

        // Description
        std::getline(ss, segment, ';');
        course.description = segment;

        courses.push_back(course);
    }
    file.close();
    return courses;
}

/**
 * @brief Searches for courses based on a filter and search term (Supports CATALOG SEARCH).
 * @param courses The vector of all courses.
 * @param filter The category to search by (e.g., "subject", "instructor", "course-code").
 * @param search_term The term to search for.
 * @return A vector of matching Course structs.
 */
std::vector<Course> search_courses(const std::vector<Course>& courses, const std::string& filter, const std::string& search_term) 
{
	std::string filter_upper = filter;
	std::string search_term_upper = search_term;
	std::transform(filter_upper.begin(), filter_upper.end(), filter_upper.begin(), [](unsigned char c1) { return std::toupper(c1); });
	std::transform(search_term_upper.begin(), search_term_upper.end(), search_term_upper.begin(), [](unsigned char c2) { return std::toupper(c2); });
	
	std::vector<Course> results;

	for (const Course& c : courses)
	{
		std::string field;
		
		if (filter_upper == "COURSE-CODE") field = c.course_code;
		
		//else if (filter_upper == "TITLE") field = c.title;
		
		else if (filter_upper == "SUBJECT") field = c.subject;
	
		else if (filter_upper == "INSTRUCTOR") field = c.instructor;
	
		std::transform(field.begin(), field.end(), field.begin(), [](unsigned char ch) { return std::toupper(ch); });
		
		if (field.find(search_term_upper) != std::string::npos)
		{
			results.push_back(c);
		}
	}
    return results;
}

/**
 * @brief Retrieves a specific course by its course code (Supports CATALOG SHOW).
 * @param courses The vector of all courses.
 * @param course_code The unique identifier for the course.
 * @return The matching Course struct, or a default constructed Course if not found.
 */
Course get_course_by_code(const std::vector<Course>& courses, const std::string& course_code) 
{
	std::string target = course_code;
	std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c) { return std::toupper(c); });

	for (const Course& c : courses)
	{
		std::string stored = c.course_code;
		std::transform(stored.begin(), stored.end(), stored.begin(), [](unsigned char ch) { return std::toupper(ch); });

		if (stored == target)
		{
			return c;
		}
	}
    return {};
}

/**
 * @brief Enrolls a student in a course, decrementing the available seats (Supports ENROLLMENT ENROLL).
 * @param courses The vector of all courses.
 * @param course_code The course to enroll in.
 * @return true if enrollment was successful, false otherwise.
 */
bool enroll_in_course(std::vector<Course>& courses, const std::string& course_code) 
{
	std::string target = course_code;
	std::transform(target.begin(), target.end(), target.begin(), [](unsigned char ch) { return std::toupper(ch); });
	
	for (Course& c : courses)
	{	
		std::string stored = c.course_code;
		std::transform(stored.begin(), stored.end(), stored.begin(), [](unsigned char ch) { return std::toupper(ch); });

		if (stored == target)
		{
			if (c.seats_available >= 1)
			{
				c.seats_available--;
				return true;
			}
			else return false;
		}
	}
	return false;
}

/**
 * @brief Drops a student from a course, incrementing the available seats (Supports ENROLLMENT DROP).
 * @param courses The vector of all courses.
 * @param course_code The course to drop.
 * @return true if drop was successful, false otherwise.
 */
bool drop_course(std::vector<Course>& courses, const std::string& course_code) 
{
	std::string target = course_code;
	std::transform(target.begin(), target.end(), target.begin(), [](unsigned char ch) { return std::toupper(ch); });
	
	for (Course& c : courses)
	{	
		std::string stored = c.course_code;
		std::transform(stored.begin(), stored.end(), stored.begin(), [](unsigned char ch) { return std::toupper(ch); });

		if (stored == target)
		{
			c.seats_available++;
			return true;
		}
	}
    return false;
}

/**
 * @brief Checks if a student meets the prerequisites for a course.
 * @param enrolled_courses A vector of courses the student is already enrolled in.
 * @param course_to_enroll The course the student wants to enroll in.
 * @return true if all prerequisites are met, false otherwise.
 */
bool check_prerequisites(const std::vector<Course>& enrolled_courses, const Course& course_to_enroll) 
{
	if (course_to_enroll.prerequisites.empty()) return true;

	for (const std::string& prereq_code : course_to_enroll.prerequisites)
    	{
		    bool found = false;
		    std::string prereq_code_upper = prereq_code;
		    std::transform(prereq_code_upper.begin(), prereq_code_upper.end(), prereq_code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
		    for (const Course& ec : enrolled_courses)
		    {
			    std::string ec_code_upper = ec.course_code;
			    std::transform(ec_code_upper.begin(), ec_code_upper.end(), ec_code_upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
			    if (ec_code_upper == prereq_code_upper)
			    {
				    found = true;
				    break;
			    }
		    }
		    if (!found)
		    {
			    return false;
		    }
	}
    return true;
}

/**
 * @brief Retrieves a student's grades for completed courses (Supports MYCOURSES VIEWGRADES).
 * @param student_name The name of the student provided during the IAM command.
 * @return A vector of GradeRecord structs representing completed courses and grades.
 */
std::vector<GradeRecord> get_student_grades(const std::string& student_name) {
    // TODO: Implement this function
    return {};
}
