//
//  main.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/20/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include <iostream>

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


const size_t HTTP_REQUEST_SIZE = PATH_MAX+32;

const int MTU = 1500; // Guess at maximum transmission unit
const int MAX_FD = 256; // Guess at maximum number of file descriptors per process

//TODO: Tune the three following constants to find optimal number of open connections and concurrent streaming files
// based on the maximum number of open file descriptors
const int MAX_CONNECTION_BACKLOG = MAX_FD/8;  // use only a portion for the backlog
const int MAX_WORKER_QUEUE_LENGTH = (MAX_FD-MAX_CONNECTION_BACKLOG)/2; // each request consumes two FD

//TODO: Tune the following two constants to find optimal throughput
const size_t INITIAL_FILEREAD_SIZE = MTU*4;
const size_t CHUNKED_FILEREAD_SIZE = MTU;

//TODO: Unkludge response status messages
const char HTTP_OK[] = "HTTP/1.0 200 OK\nContent-Type: application/octet-stream\n\n";
const size_t HTTP_OK_SIZE = sizeof(HTTP_OK)-1;
const char HTTP_404[] = "HTTP/1.0 404 Not Found\n\n";
const size_t HTTP_404_SIZE = sizeof(HTTP_404)-1;
const char HTTP_500[] = "HTTP/1.0 500 Error\n\n";
const size_t HTTP_500_SIZE = sizeof(HTTP_500)-1;

class HttpPool {
    std::vector<std::thread> workers;
    std::deque<std::function<void()> > requests;
    std::condition_variable request_ready;
    std::atomic<bool> shutdown;
    std::mutex task_queue_mutex;
public:
    HttpPool(size_t number_threads) : shutdown(false) {
        // lambdas for easy coding
        for(int i=0; i < number_threads; i++) {
            workers.push_back(std::thread([this]() {
                std::function<void()> request;
                // loop infinitely since we will be notified of exit by another thread
                for(;;) {
                    // for safety do the locking with a control block
                    {
                        std::unique_lock<std::mutex> lock(task_queue_mutex);
                        
                        request_ready.wait(lock, [this]() {
                            return !(requests.size() == 0 && !shutdown);
                        });
                        
                        if ( shutdown )
                            return;
                        
                        request = requests.front();
                        requests.pop_front();
                        
                    } // lock released
                    request();
                }
                
                return;
            }));
        }
    }
    
    bool enqueue(std::function<void()> func) {
        bool retVal = false;
        std::unique_lock<std::mutex> lock(task_queue_mutex);

        if (requests.size() < MAX_WORKER_QUEUE_LENGTH ) {
            requests.push_back(func);
            retVal = true;
            request_ready.notify_one();
        }
        return retVal;
    }
    
};

void send_file_chunk(int socket, int file, const size_t chunk_size, HttpPool & pool) {
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
        bool queued = pool.enqueue(std::bind(send_file_chunk,socket,file,CHUNKED_FILEREAD_SIZE,std::ref(pool)));
        if ( !queued ) {
            // TODO: Need better error logging
            std::cerr << "error queueing some unknown file " << errno;
        }
    }
    
}


// TODO: unkludge this since it ignores all the headers and only looks for GET and returns 500 for everything else
void read_http_request(int socket,sockaddr_in client_addr, HttpPool & pool) {
    char request[HTTP_REQUEST_SIZE];
    
    size_t request_read = ::read(socket,(void *)request,HTTP_REQUEST_SIZE);
    if (request_read > 6 ) {
        
        // make the first 3 characters upper case
        for(int i=0; i < 3; i++)
            request[i] = toupper(request[i]);
        
        int starting_character = 4;
        if ( strncmp(request, "GET ", starting_character) == 0 ) {
            // reuse the request input buffer so we are going to sliceup the buffer
            // turn "get /someurl HTTP/1.1" into "./someurl"
            starting_character -= 1;
            int ending_character = starting_character;
            request[ending_character] = '.';
            while ( ending_character < request_read) {
                if (request[ending_character] == ' ' ||  request[ending_character] == '\n' )
                    break;
                ending_character += 1;
            }
            request[ending_character] = '\0';
            
            // TODO: this is pretty bad security risk size we preserve the user input string
            std::cout<< std::this_thread::get_id() << ":" << client_addr.sin_addr.s_addr << " " << request_read << " " << request+starting_character << "." << std::endl;
            
            int file = ::open(request+starting_character, O_RDONLY);
            if ( file > -1 ) {
                ::write(socket,HTTP_OK,HTTP_OK_SIZE);
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
    }
    else {
        ::write(socket,HTTP_500,HTTP_500_SIZE);
        ::close(socket);
    }
    
    return;
}


int main(int argc, const char * argv[]) {
    //TODO: remove security risk by having static content server. CDN perhaps?
    
    char *cwd = getcwd(NULL, 0);
    
    std::cout << "Starting in directory " << cwd << std::endl;
    
    // create listening socket
    std::cout << "Creating socket" << std::endl;
    int listen_on = socket(PF_INET,SOCK_STREAM,0);
    if ( listen_on > 0 ) {
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
                HttpPool pool(std::thread::hardware_concurrency()-1);
                
                std::cout << "Accepting socket" << std::endl;
                while ( client_socket != -1 )
                {
                    client_socket = accept(listen_on, (sockaddr *)&client_addr, &client_addr_length);
                    bool queued = pool.enqueue(std::bind(read_http_request,client_socket,client_addr,std::ref(pool)));
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
