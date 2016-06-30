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
