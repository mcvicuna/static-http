//
//  main.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/20/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include <iostream>
#include <fstream>

#include <thread>
#include <vector>
#include <deque>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslimits.h>

// OSX TCP/IP socket addr header
#include <netinet/in.h>

#include "HttpConnection.h"
#include "HttpPool.h"
#include "HttpRouting.h"
#include "HttpContenttypes.h"







#if 0



void send_file_chunk(int socket, int file, const size_t chunk_size) {
    char buffer[chunk_size];
    
    size_t bytes_in = read(file,buffer,chunk_size);
    if ( bytes_in < chunk_size ) {
        if ( bytes_in > 1 )
            write(socket,buffer,bytes_in);
        else // TODO: Need better error logging
            std::cerr << "error reading some unknown file " << errno;
        close(file);
        close(socket);
    }
    else {
        write(socket,buffer,bytes_in);
        bool queued = HttpPool::get().enqueue(std::bind(send_file_chunk,socket,file,CHUNKED_FILEREAD_SIZE));
        if ( !queued ) {
            // TODO: Need better error logging
            std::cerr << "error queueing some unknown file " << errno;
        }
    }
    
}

// TODO: unkludge this since it ignores all the headers and only looks for GET and only responds with 404 or 500 when it encounters an error
// TODO: Only supports the Content-Type of "application/octet-stream"
void http_request(HttpConnection http) {
    char request[HTTP_REQUEST_SIZE];
    
    size_t request_read = ::read(socket,(void *)request,HTTP_REQUEST_SIZE);
    if (request_read > 6 ) {
        
        // make the first 3 characters upper case
        for(int i=0; i < 3; i++)
            request[i] = toupper(request[i]);
        
        int starting_character = 5;
        if ( strncmp(request, "GET /", starting_character) == 0 ) {
            // reuse the request input buffer so we are going to sliceup the buffer
            // turn "get resource HTTP/1.1\n[headers]\n\n" into "get resource" and find offset to the first character
            // after the /

            int ending_character = starting_character;
            while ( ending_character < request_read) {
                if (request[ending_character] == ' ' ||  request[ending_character] == '\n' )
                    break;
                ending_character += 1;
            }
            request[ending_character] = '\0';
            

#ifdef DEBUG
 //           std::cout<< std::this_thread::get_id() << ":" << client_addr.sin_addr.s_addr << " " << request_read << " " << request+starting_character << "." << std::endl;
#endif
            // TODO: this is pretty bad security risk size we preserve the user input string
            char *filename = request+starting_character;
             // send the content type
            std::string content_type = get_content_type(filename);
            if ( content_type.length()) {
            
                int file = ::open(filename, O_RDONLY);
                if ( file > -1 ) {
                    ::write(socket,HTTP_OK,HTTP_OK_SIZE);
                    // send the content type
                    ::write(socket,HTTP_CONTENT_TYPE,HTTP_CONTENT_TYPE_SIZE);
                    ::write(socket,content_type.c_str(),content_type.length());
                    ::write(socket,HTTP_END_OF_HEADER,HTTP_END_OF_HEADER_SIZE);
                    
                    bool queued = pool.enqueue(std::bind(send_file_chunk,socket,file,INITIAL_FILEREAD_SIZE,std::ref(pool)));
                    if ( !queued ) {
                        // TODO: Need better error logging
                        std::cerr << "error queueing reads" <<  std::endl;
                        ::close(socket);
                        
                    }
                }
                else {
                    ::write(socket,HTTP_404, HTTP_404_SIZE);
                    // TODO: Need better error logging
                    std::cerr << "error opening some file " << errno << " " << request+starting_character <<  std::endl;
                    ::close(socket);
                }
            }
            else { // cant find content type
                ::write(socket,HTTP_500,HTTP_500_SIZE);
                ::close(socket);
            }

        }
    }
    else {
        ::write(socket,HTTP_500,HTTP_500_SIZE);
        ::close(socket);
    }
    
    return;
}
#endif

class DefaultRoute : public HttpRoute {
    virtual bool do_handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) {
        
        
        
        std::unique_ptr<std::ifstream> file = std::unique_ptr<std::ifstream>(new std::ifstream());
        std::string filename = params[0].substr(1);
        // std::cerr << "Default " << request.resource << "->" << filename << std::endl;
        file->open(filename,std::ios_base::binary | std::ios_base::in);
        if ( file->is_open() ) {
        
        
            request->set_status(HTTP_OK, "OK");
        
            std::string content_type = HttpContentTypes::get().get_content_by_ext(params[0].c_str());
            
            request->write_header(HTTP_HEADER_CONTENT_TYPE, content_type);
            
        }
        else {
            request->set_status(HTTP_NOT_FOUND, "Not found");
        }
        
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_response,request.release(),file.release()));
        
        return true;
    };
};

class EchoRoute : public HttpRoute {
    virtual bool do_handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) {
        std::cerr << "EchoRoute " << request->resource << "?" << params[0];
        
        request->set_status(HTTP_OK, "OK");
        request->write_header(HTTP_HEADER_CONTENT_TYPE, HttpContentTypes::get().get_content_by_name("html"));
        std::unique_ptr<std::stringstream> response(new std::stringstream(params[0]));
        
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_response,request.release(),response.release()));

        return true;
    };
};

class FriendRoute : public HttpRoute {
    virtual bool do_handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) {
        request->set_status(HTTP_OK, "OK");
        request->write_header(HTTP_HEADER_CONTENT_TYPE, HttpContentTypes::get().get_content_by_name("html"));
        
        std::unique_ptr<std::stringstream> response(new std::stringstream());
        *response << "How would I know if " << params[0] << " and " << params[1] << " are friends " << std::endl;
        
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_response,request.release(),response.release()));

        return true;
    };
};


void setup_routes() {
    HttpRouting::get().addRoute("GET", "([^?]+[^\\/]$)", std::unique_ptr<DefaultRoute>(new DefaultRoute()));
    HttpRouting::get().addRoute("GET", "\\/echo\\?(.+)", std::unique_ptr<EchoRoute>(new EchoRoute()));
    HttpRouting::get().addRoute("GET", "\\/friend\\/([^/]+)\\/([^/]+)\\/", std::unique_ptr<FriendRoute>(new FriendRoute()));
    

}

int main(int argc, const char * argv[]) {
    
    char *cwd = getcwd(NULL, 0);
    
    std::cout << "Starting in directory " << cwd << std::endl;
    
    // create listening socket
    std::cout << "Creating socket" << std::endl;
    int listen_on = socket(PF_INET,SOCK_STREAM,0);
    if ( listen_on > 0 ) {
        int reuse_flag = 1;
        setsockopt(listen_on, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag));
        sockaddr_in bind_addr;
        
        bzero(&bind_addr, sizeof(bind_addr));
        // assume we are going to listen on all interfaces on the 8080
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = htons(8080);
        std::cout << "Binding socket" << std::endl;
        if ( bind(listen_on,(sockaddr *)&bind_addr,sizeof(bind_addr)) == 0) {
            // now start listening and accepting connections
            std::cout << "Listening on socket" << std::endl;
            if ( listen(listen_on,MAX_CONNECTION_BACKLOG) == 0 ) {
                sockaddr_in client_addr;
                socklen_t client_addr_length = sizeof(client_addr);
                int client_socket = 0;
                
                setup_routes();
                
                std::cout << "Accepting socket" << std::endl;
                while ( client_socket != -1 )
                {
                    client_socket = accept(listen_on, (sockaddr *)&client_addr, &client_addr_length);
//                     std::cout << "new connection" << client_socket << std::endl;
                    // check return of accept
                    if ( client_socket < 0 ) {
                        //TODO: Log error
                        break;
                    }
//                    int status = fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);
//                    if ( status == -1 ) {
//                        std::cerr << "Cant make socket noblocking " << errno << std::endl;
//                        break;
//                    }
                    HttpConnection *connection = new HttpConnection(client_socket);
                    bool queued = HttpPool::get().enqueue(std::bind(&HttpConnection::read,connection));
                    if ( !queued ) {
                        std::cerr << "Failed to enqueue request\n" << std::endl;
                        break;
                    }
                    
                }
                
            }
            
        }
        
    }
    
    // TODO: more robust error reporting
    
    std::cerr << "Exiting with status " << errno << std::endl;
    
    return errno;
}
