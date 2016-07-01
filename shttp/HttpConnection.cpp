//
//  HttpConnection.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/29/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include "HttpConnection.h"
#include "HttpResponses.h"
#include <ostream>

#include <stdio.h>
#include <time.h>


const int MTU = 1500; // Guess at maximum transmission unit

//TODO: Tune the following two constants to find optimal throughput
const size_t INITIAL_FILEREAD_SIZE = MTU*4;
const size_t CHUNKED_FILEREAD_SIZE = MTU;

void HttpConnection::read(HttpConnection *connection) {
    std::unique_ptr<HttpConnection> conn(connection);
    char input[HTTP_REQUEST_SIZE];
    
    //switch to time based idle timeout
    if (  time(NULL) > conn->last_request+MAX_IDLE_TIMEOUT ) {
        conn->close = true;
        std::cerr << "connection idle " << time(NULL) << "<" << conn->last_request+MAX_IDLE_TIMEOUT << std::endl;
        HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));
        return;
    }
    
    int poll_status = poll(&conn->fds,1,0);
    if ( poll_status == -1 ) {
        // delete myself
        std::cerr << "Bad poll results" << std::endl;
        conn->close = true;
        HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));
    }
    else if ( poll_status != 0 )
    {
        if ( conn->fds.revents & (POLLHUP | POLLERR) ) {
            // delete myself
            // std::cerr << "Bad poll event" << conn->fds.revents << std::endl;
            conn->close = true;
            HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));
        }
        else {
            size_t request_read = ::read(conn->fds.fd,(void *)input,HTTP_REQUEST_SIZE);
            if ( request_read == -1 ) {
                // delete myself
                //std::cerr << "Bad poll read" << errno << std::endl;
                conn->close = true;
                HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));
            }
            else if ( request_read != 0 ) {
                // reset timeout so slow connections don't timeout in middle of requests
                conn->last_request = time(NULL);
                conn->bytes_read += request_read;
                conn->buffer.write(input,request_read);
                // seach for the "\n\n"
                conn->request = conn->buffer.str();
                
                auto end = conn->request.find(HTTP_HEADER_END);
                if ( end != std::string::npos ) {
                    // put the remaing data back into the buffer
                    conn->buffer.str(conn->request.substr(end+4)); // add 4 for the terminators
                    // trucate the data so it only include the extra data
                    conn->request = conn->request.substr(0,end);
                    // std::cerr << "?" << request << std::endl;
                    conn->parse_request(conn->request);

                    std::cerr << "Handling " << ++conn->requests_handled << " " << conn->resource << std::endl;

                    // send this off to the router
                    HttpRouting::get().route(std::move(conn));
                        

                }
                else  {
                    std::cerr << "multiple packets " << std::endl;
                    HttpPool::get().enqueue(std::bind(&HttpConnection::read, conn.release()));
                }
            }
            
            
            else {
                //std::cerr << "multiple request_read packets " << std::endl;
                HttpPool::get().enqueue(std::bind(&HttpConnection::read, conn.release()));
                
            }
        }
    }
    else {
            //std::cerr << "multiple poll_status packets " << std::endl;
            HttpPool::get().enqueue(std::bind(&HttpConnection::read, conn.release()));
        
    }
    
}


void HttpConnection::parse_request(std::string request) {
    std::istringstream request_stream(request);
    std::string command;
    // split the first line
    if ( getline(request_stream,command,'\n')) {
        std::vector<std::string> pieces;
        std::istringstream command_stream(command);
        std::string temp_piece;
        while( getline(command_stream,temp_piece,' ') ) {
            pieces.push_back(temp_piece);
        }
        if ( pieces.size() == 3) {
            method = pieces[0];
            resource = pieces[1];
            if ( pieces[2].compare(0,HTTP_V1_0.size(),HTTP_V1_0) != 0) {
                close = false;
            }
            
            // parse out all the header fields
            std::string header;
            while ( getline(request_stream,header,'\n')) {
                auto found_delim = header.find(':');
                if ( found_delim != std::string::npos ) {
                    auto found_value = header.substr(found_delim+1).find_first_not_of(" ");
                    if ( found_value != std::string::npos ) {
                        auto placed = headers.emplace(std::make_pair(header.substr(0,found_delim),
                                                                     header.substr(found_delim+1+found_value)));
                        //std::cerr << placed.first->first << ":" << placed.first->second << std::endl;
                        
                        if ( !placed.second ) {
                            // TODO: header not inserted
                            std::cerr << "not inserted "<< placed.first->first << ":" << placed.first->second << std::endl;
                            
                        }
                    }
                }
                else {
                    // TODO: bad head field
                    // assume its the body?
                    std::cerr << "Parsing body? " << std::endl;
                    break;
                }
            }
            // check to see if this header is the last header
            auto last_request = headers.find(HTTP_HEADER_CONNECTION);
            if ( last_request != headers.end() ) {
                if ( HTTP_HEADER_CONNECTION_CLOSE.compare(0,HTTP_HEADER_CONNECTION_CLOSE.size(),last_request->second) == 0)
                    close = true;
            }
        }
        else {
            // TODO: bad command line
            std::cerr << "Parsing command " << command <<std::endl;
        }
        
    }
    else {
        // TODO: bad request header.
        std::cerr << "Parsing command " << command <<std::endl;
    }
    
    
    
    
}

void HttpConnection::write_response(HttpConnection *connection) {
    std::unique_ptr<HttpConnection> conn(connection);

    conn->response << HTTP_V1_1 << " " << conn->status_code << " " << conn->status_reason << HTTP_ENDL;
    size_t body_length = 0;
    
    if ( conn->body != nullptr && !conn->body->fail() ) {
        // get length of result:
        conn->body->seekg (0, std::ios_base::end);
        body_length = conn->body->tellg();
        conn->body->seekg (0, std::ios_base::beg);

    }
    
    conn->write_header(HTTP_HEADER_CONTENT_LENGTH, body_length);
    char buf[1000];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    
    conn->write_header(HTTP_HEADER_DATE, buf);
    
    if ( conn->close ) {
        conn->write_header(HTTP_HEADER_CONNECTION, HTTP_HEADER_CONNECTION_CLOSE);
    }
    else {
        conn->write_header(HTTP_HEADER_CONNECTION, HTTP_HEADER_CONNECTION_KEEPALIVE);
    }
        
    conn->response << conn->response_headers.str() << HTTP_ENDL;
    
    std::string header = conn->response.str();
    
    ::write(conn->fds.fd,header.c_str(),header.length());
    if ( body_length )
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_body, conn.release(), INITIAL_FILEREAD_SIZE));
    else
        HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));

}

void HttpConnection::set_status(int code, const std::string reason)
{
    status_code = code;
    status_reason = reason;
    
}

void HttpConnection::write_body(HttpConnection *connection, const size_t chunk_size) {
    std::unique_ptr<HttpConnection> conn(connection);
    char buffer[chunk_size];
    
    conn->body->read(buffer,chunk_size);
    if ( !conn->body->fail() ) {
        ::write(conn->fds.fd,buffer,chunk_size);
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_body, conn.release(),CHUNKED_FILEREAD_SIZE));
    }
    else {
        
        if ( conn->body->gcount() > 0 ) {
            ::write(conn->fds.fd,buffer,conn->body->gcount());
            //::write(conn->fds.fd,HTTP_HEADER_END.c_str(),HTTP_HEADER_END.size());
        }
        // clean up
        HttpPool::get().enqueue(std::bind(&HttpConnection::clean_up,conn.release()));

    }
    
}

// called after request has been serviced on HTTP 1.1 connections
void HttpConnection::clean_up(HttpConnection *connection) {
    std::unique_ptr<HttpConnection> clean_up(connection);
    
    if ( clean_up->close ) {
        ::close(clean_up->fds.fd);
    }
    else {
        clean_up->response.clear();
        clean_up->response.str(std::string());
        clean_up->response_headers.clear();
        clean_up->response_headers.str(std::string());
        clean_up->headers.clear();
        clean_up->set_status(0, "");
        clean_up->last_request = time(NULL);
        clean_up->body.reset(nullptr);
        HttpPool::get().enqueue(std::bind(&HttpConnection::read,clean_up.release()));
    }
    
}

