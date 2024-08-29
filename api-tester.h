//
// Created by A. Kevin Bailey on 8/10/2024 under a GPL3.0 license
//
#ifndef api_tester_H
#define api_tester_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#undef CURL_MAX_WRITE_SIZE
#define CURL_MAX_WRITE_SIZE 1048576 // Maximum bodies size in bytes
#define MAX_HEADERS 50 // Maximum number of headers
#define MAX_BODY_PARTS 1 // Maximum parts if we are expecting a multipart message
#define MAX_RESPONSE_CODE_LENGTH 4 // Maximum character length of the HTTP status code plus null terminator
#define MAX_RESPONSE_PHRASE_LENGTH 41 // Maximum character length of the HTTP status phrase plus null terminator
//#define MAX_LINE_LENGTH 81 // Maximum character width to the screen plus null terminator

// Implementation of the string HTTP status codes, because the CURL libraries do not have them.
typedef struct {
    int code;
    const char *phrase;
} status_code_t;

// Structure to hold headers
typedef struct {
    char *headers[MAX_HEADERS]; // Array of pointers to hold header strings
    int count;                  // Number of headers collected
} header_t;

// Structure to hold HTTP response body
typedef struct {
    char *body[MAX_BODY_PARTS]; // Array of pointers to hold body strings (for single part messages this is 1)
    int count;     // Number of body parts collected
} body_t;

// Structure to pass to the threads
typedef struct {
    CURL *pCurl;
    pthread_mutex_t *mutex;
    double *responseTimes;
    const char *url;
    int sleepTime;
    int keepConnectsOpen;
    int reuseConnects;
    int threadID;
    int numCalls;
} thread_data_t;

// Implementation of the string HTTP status codes, because the CURL libraries do not have them.
void http_status_phrase(char*responsePhrase, long statusCode);

// Create custom function to return the difference tin milliseconds, because difftime in <time.h> only returns seconds.
double time_diff_ms(struct timespec *start, struct timespec *end);

// Create custom parse int function to help with error handling.
int parse_int(const char *str, const char *argName);

// Curl callback function to handle the response header data.
// Also, this function is called on every header element.  Therefore, we must collect the headers together.
size_t header_callback(const char *buffer, size_t charSize, size_t dataSize, void *userData);

// Curl callback function to handle the response body data
size_t body_callback(const char *buffer, size_t charSize, size_t dataSize, void *userData);

// Curl callback function to dump the response header data and keep the connection occupied.
size_t dump_header_callback(void *buffer, size_t charSize, size_t dataSize, void *userData);

// Curl callback function to dump the response body data and keep the connection occupied.
size_t dump_body_callback(void *buffer, size_t charSize, size_t dataSize, void *userData);

// Initialize the map with common HTTP status codes and phrases
const status_code_t status_code_map[] = {
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {417, "Expectation Failed"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Content"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {500, "Internal Server Error"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {511, "Network Authentication Required"}
        // Add more status codes as needed
};

const int statusCodeMapSize = sizeof(status_code_map) / sizeof(status_code_t);

#endif //api_tester_H
