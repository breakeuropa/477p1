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
std::vector<Course> search_courses(const std::vector<Course>& courses, const std::string& filter, const std::string& search_term) {
    // TODO: Implement this function
    return {};
}

/**
 * @brief Retrieves a specific course by its course code (Supports CATALOG SHOW).
 * @param courses The vector of all courses.
 * @param course_code The unique identifier for the course.
 * @return The matching Course struct, or a default constructed Course if not found.
 */
Course get_course_by_code(const std::vector<Course>& courses, const std::string& course_code) {
    // TODO: Implement this function
    return {};
}

/**
 * @brief Enrolls a student in a course, decrementing the available seats (Supports ENROLLMENT ENROLL).
 * @param courses The vector of all courses.
 * @param course_code The course to enroll in.
 * @return true if enrollment was successful, false otherwise.
 */
bool enroll_in_course(std::vector<Course>& courses, const std::string& course_code) {
    // TODO: Implement this function
    return false;
}

/**
 * @brief Drops a student from a course, incrementing the available seats (Supports ENROLLMENT DROP).
 * @param courses The vector of all courses.
 * @param course_code The course to drop.
 * @return true if drop was successful, false otherwise.
 */
bool drop_course(std::vector<Course>& courses, const std::string& course_code) {
    // TODO: Implement this function
    return false;
}

/**
 * @brief Checks if a student meets the prerequisites for a course.
 * @param enrolled_courses A vector of courses the student is already enrolled in.
 * @param course_to_enroll The course the student wants to enroll in.
 * @return true if all prerequisites are met, false otherwise.
 */
bool check_prerequisites(const std::vector<Course>& enrolled_courses, const Course& course_to_enroll) {
    // TODO: Implement this function
    return false;
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