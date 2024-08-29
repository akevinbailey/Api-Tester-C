//
// Created by A. Kevin Bailey on 8/10/2024 under a GPL3.0 license
//
#include "api-tester.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
void print_help() {
    printf("Usage:\n");
    printf("  api-tester [URL] [arguments]\n");
    printf("Required arguments:\n");
    printf("  [URL]                   - Server URL.\n");
    printf("Optional Arguments:\n");
    printf("  -totalCalls [value]     - Total number of calls across all threads. Default is 10000.\n");
    printf("  -numThreads [value]     - Number of threads. Default is 12.\n");
    printf("  -sleepTime [value]      - Sleep time in milliseconds between calls within a thread. Default is 0.\n");
    printf("  -requestTimeOut [value] - HTTP request timeout in milliseconds. Default is 10000.\n");
    printf("  -connectTimeOut [value] - HTTP request timeout in milliseconds. Default is 20000.\n");
    printf("  -reuseConnects          - Attempts to reuse the connections if the server allows it.\n");
    printf("  -keepConnectsOpen       - Force a new connection with every request (not advised).\n");
    printf("Help:\n");
    printf("  -? or --help            - Display this help message.\n");
}
#pragma clang diagnostic pop

// Implementation of the string HTTP status codes, because the CURL libraries do not have them.
void http_status_phrase(char*responsePhrase, long statusCode) {

    ltoa(statusCode, responsePhrase, 10);

    for (int i = 0; i < statusCodeMapSize; i++) {
        if (status_code_map[i].code == statusCode) {
            strcat(responsePhrase, " ");
            strcat(responsePhrase, status_code_map[i].phrase);
            return;
        }
    }

    strcat(responsePhrase, " Unknown Status Code");
}

// Create custom function to return the difference tin milliseconds, because difftime in <time.h> only returns seconds.
double time_diff_ms(struct timespec *end, struct timespec *start) {
    double diffMs = ((double)end->tv_sec - (double)start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1000000.0;
    return diffMs;
}

// Create custom parse int function to help with error handling.
int parse_int(const char *str, const char *argName) {
    char *pEndStr;
    errno = 0;
    long val = strtol(str, &pEndStr, 10);

    if (errno != 0 || *pEndStr != '\0' || str == pEndStr) {
        fprintf(stderr, "Error: \"%s\" is not a valid integer for %s.\n", str, argName);
        print_help();
        exit(EXIT_FAILURE);
    }

    return val;
}

// Curl callback function to handle the response header data.
// Also, this function is called on every header element.  Therefore, we must collect the headers together.
size_t header_callback(const char *buffer, size_t charSize, size_t dataSize, void *userData) {
    size_t headerLength = dataSize * charSize;
    header_t *headerData = (header_t*)userData;

    // Must read the headers to close the session.
    if (headerData->count < MAX_HEADERS) {
        headerData->headers[headerData->count] = malloc(headerLength + 1);  // Don't forget to free this memory!!
        if (headerData->headers[headerData->count]) {
            strncpy(headerData->headers[headerData->count], buffer, headerLength);
            headerData->headers[headerData->count][headerLength] = '\0';
            headerData->count++;
        }
    }

    return headerLength;
}

// Curl callback function to handle the response body data
size_t body_callback(const char *buffer, size_t charSize, size_t dataSize, void *userData) {
    size_t bodyLength = dataSize * charSize;
    body_t *bodyData = (body_t*)userData;

    // Must read the body to close the session.
    if (bodyData->count < MAX_BODY_PARTS) {
        bodyData->body[bodyData->count] = malloc(bodyLength + 1); // Don't forget to free this memory!!
        if (bodyData->body[bodyData->count]) {
            strncpy(bodyData->body[bodyData->count], buffer, bodyLength);
            bodyData->body[bodyData->count][bodyLength] = '\0';
            bodyData->count++;
        }
    }
    return bodyLength;
}

// Curl callback function to dump the response header data and keep the connection occupied.
size_t dump_header_callback(void *buffer, size_t charSize, size_t dataSize, void *userData) {
    // Does not read the headers, thus keeping the connection occupied until the connection timeout.
    (void)buffer; // Used to suppress unused parameters warning
    (void)userData; // Used to suppress unused parameters warning
    return charSize * dataSize;
}

// Curl callback function to dump the response body data and keep the connection occupied.
size_t dump_body_callback(void *buffer, size_t charSize, size_t dataSize, void *userData) {
    // Does not read the body, thus keeping the connection occupied until the connection timeout.
    (void)buffer; // Used to suppress unused parameters warning
    (void)userData; // Used to suppress unused parameters warning
    return charSize * dataSize;
}

// Makes the HTTP call in its own thread
void *fetch_data(void *threadData) {
    thread_data_t *data = (thread_data_t *)threadData;
    CURL *pCurl = data->pCurl;
    pthread_mutex_t *mutex = data->mutex;
    double *responseTimes = data->responseTimes;
    const char *url = data->url;
    int sleepTime = data->sleepTime;
    int keepConnectsOpen = data->keepConnectsOpen;
    int reuseConnects = data->reuseConnects;
    int threadID = data->threadID;
    int numCalls = data->numCalls;

    for (int i = 0; i < numCalls; i++) {
        double responseTime;
        long responseCode;
        char *responsePhrase = malloc(MAX_RESPONSE_CODE_LENGTH + MAX_RESPONSE_PHRASE_LENGTH);
        CURLcode res;
        struct timespec start, end;
        header_t header;
        body_t body;

        // Initialize the header and body count.
        header.count = 0;
        body.count = 0;

        if (keepConnectsOpen) {
            // Does not read the headers and body, thus keeping the connection occupied until the connection timeout.
            curl_easy_setopt(pCurl, CURLOPT_HEADERFUNCTION , dump_header_callback);
            curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, dump_body_callback);
        } else {
            // Must read the headers and body to close the session.
            curl_easy_setopt(pCurl, CURLOPT_HEADERFUNCTION , header_callback);
            curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, body_callback);
            curl_easy_setopt(pCurl, CURLOPT_HEADERDATA, &header);
            curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &body);
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        res = curl_easy_perform(pCurl);
        clock_gettime(CLOCK_MONOTONIC, &end);
        responseTime = time_diff_ms(&end, &start);

        curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &responseCode);
        http_status_phrase(responsePhrase, responseCode);

        // Keep the mutex lock time to a minimum.
        pthread_mutex_lock(mutex);
            if (res == CURLE_OK) {
                printf("Thread %2d.%-6d - Success: %s - Response time: %.2f ms\n", threadID, i, responsePhrase, responseTime);
            } else {
                fprintf(stderr, "Thread %2d.%-6d - Request failed: %s - Response time: %.2f ms\n", threadID, i, curl_easy_strerror(res), responseTime);
            }
            responseTimes[i] = responseTime;
        pthread_mutex_unlock(mutex);

        // Free up the allocated memory.
        for (int j = 0; j < header.count; j++) {
            // Free up memory from the header_callback strings.
            curl_free(header.headers[j]);
        }
        for (int j = 0; j < body.count; j++){
            // Free up memory from the body_callback strings.
            curl_free(body.body[j]);
        }
        free(responsePhrase);

        usleep(sleepTime * 1000);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: No command line argument provided.\n");
        print_help();
        return EXIT_FAILURE;
    }

    const char *url = NULL;
    int totalCalls = 10000;
    int numThreads = 12;
    int sleepTime = 0;
    int requestTimeOut = 10000;
    int connectTimeOut = requestTimeOut * 3;
    int reuseConnects = 0;
    int keepConnectsOpen = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return EXIT_SUCCESS;
        } else if (i == 1) {
            url = argv[i];
        } else if (strcmp(argv[i], "-totalCalls") == 0 && i + 1 < argc) {
            totalCalls = parse_int(argv[++i], "-totalCalls");
        } else if (strcmp(argv[i], "-numThreads") == 0 && i + 1 < argc) {
            numThreads = parse_int(argv[++i], "-numThreads");
        } else if (strcmp(argv[i], "-sleepTime") == 0 && i + 1 < argc) {
            sleepTime = parse_int(argv[++i], "-sleepTime");
        } else if (strcmp(argv[i], "-requestTimeOut") == 0 && i + 1 < argc) {
            requestTimeOut = parse_int(argv[++i], "-requestTimeOut");
        } else if (strcmp(argv[i], "-connectTimeOut") == 0 && i + 1 < argc) {
            connectTimeOut = parse_int(argv[++i], "-connectTimeOut");
        } else if (strcmp(argv[i], "-reuseConnects") == 0) {
            reuseConnects = 1;
        } else if (strcmp(argv[i], "-keepConnectsOpen") == 0) {
            keepConnectsOpen = 1;
        }
    }

    if (url == NULL || strncmp(url, "http", 4) != 0) {
        fprintf(stderr, "Error: \"%s\" is not a valid URL\n", url);
        print_help();
        return EXIT_FAILURE;
    }

    CURL *pCurls[numThreads];
    pthread_t threads[numThreads];
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);  //Don't forget to free this memory!!
    double *responseTimes = malloc(totalCalls * sizeof(double)); // Don't forget to free this memory!!

    // Put in the connection HTTP header so the server can try to fulfill our request.
    struct curl_slist *curlHeaders = NULL;  // Don't forget to free this memory!!
    if (reuseConnects) {
        curlHeaders = curl_slist_append(curlHeaders, "Connection: keep-alive");
    } else {
        curlHeaders = curl_slist_append(curlHeaders, "Connection: close");
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    for (long i = 0; i < numThreads; i++) {
        pCurls[i] = curl_easy_init();
        curl_easy_setopt(pCurls[i], CURLOPT_URL, url);
        curl_easy_setopt(pCurls[i], CURLOPT_HTTPGET, 1);
        curl_easy_setopt(pCurls[i], CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(pCurls[i], CURLOPT_TIMEOUT_MS, requestTimeOut);
        curl_easy_setopt(pCurls[i], CURLOPT_CONNECTTIMEOUT_MS, connectTimeOut);
        curl_easy_setopt(pCurls[i], CURLOPT_TCP_KEEPALIVE, reuseConnects);
        curl_easy_setopt(pCurls[i], CURLOPT_FORBID_REUSE, keepConnectsOpen);
        curl_easy_setopt(pCurls[i], CURLOPT_HTTPHEADER, curlHeaders);

    }

    long callsPerThread = totalCalls / numThreads;
    long remainderCalls = totalCalls % numThreads;

    struct timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    for (long i = 0; i < numThreads; i++) {
        // Add one call to each thread number that is less than the mod of the total calls to compensate for the remainder
        long numCalls = callsPerThread + (i < remainderCalls ? 1 : 0);
        thread_data_t *data = malloc(sizeof(thread_data_t));
        data->pCurl = pCurls[i];
        data->mutex = &mutex;
        data->responseTimes = &responseTimes[i * numCalls];
        data->url = url;
        data->sleepTime = sleepTime;
        data->keepConnectsOpen = keepConnectsOpen;
        data->reuseConnects = reuseConnects;
        data->threadID = i;
        data->numCalls = numCalls;

        pthread_create(&threads[i], NULL, fetch_data, data);
    }

    for (long i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
        curl_easy_cleanup(pCurls[i]);
    }

    struct timespec endTime;
    clock_gettime(CLOCK_MONOTONIC, &endTime);

    // Total execution time in seconds
    double totalTime = time_diff_ms(&endTime, &startTime) / 1000; // Convert to seconds

    double requestsPerSecond = totalCalls / totalTime ;

    double totalResponseTime = 0;
    for (long i = 0; i < totalCalls; i++) {
        totalResponseTime += responseTimes[i];
    }
    double averageResponseTime = totalResponseTime / totalCalls;

    printf("Total thread count: %d\n", numThreads);
    printf("Total test time: %.2f s\n", totalTime);
    printf("Average response time: %.2f ms\n", averageResponseTime);
    printf("Average requests per second: %.2f\n", requestsPerSecond);

    curl_slist_free_all(curlHeaders);
    free(responseTimes);
    pthread_mutex_destroy(&mutex);
    curl_global_cleanup();

    printf("All threads have finished.\n");

    return EXIT_SUCCESS;
}