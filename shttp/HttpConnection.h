//
//  HttpConnection.h
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpConnection_h
#define HttpConnection_h

#include <string>
#include <map>

#include <iostream>
#include <sstream>
#include <atomic>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslimits.h>

// OSX TCP/IP socket addr header
#include <netinet/in.h>

#include <poll.h>

#include "HttpPool.h"
#include "HttpRouting.h"

class HttpConnection {
private:
    struct pollfd fds;

    std::ostringstream buffer;
    std::string request;
    bool close;
    std::atomic_bool closed;
    int status_code;
    std::string status_reason;
    int bytes_read;
    int requests_handled;
    int idle_tries;
    
    std::ostringstream response;
    std::ostringstream response_headers;
    
    const static size_t HTTP_REQUEST_SIZE = 1500;
    const static int MAX_IDLE_TRIES = 100000;
    
public:
    std::string method;
    std::string resource;
    std::map<std::string, std::string> headers;
    std::string body;


private:
    void parse_request(std::string request);
public:
    
    HttpConnection(int socket) : closed(false), close(true), bytes_read(0), idle_tries(0),requests_handled(0) {
        fds.fd = socket;
        fds.events = POLLIN|POLLPRI;
    }
    

    template <typename tokenT, typename valueT> void write_header(tokenT token,valueT value) {
            response_headers << token << ": " << value << HTTP_ENDL;
    }
    
    void set_status(int code, const std::string reason);
   
    static void read(HttpConnection *connection);
    static void write_response(HttpConnection *connection,std::istream *_body=nullptr);
    static void write_body(HttpConnection *connection,std::istream *_body,const size_t chunk_size);
    
    static void clean_up(HttpConnection *connection);
    
    ~HttpConnection() = default;
};
#endif /* HttpConnection_h */
