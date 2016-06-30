//
//  HttpRouting.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include <iostream>

#include <algorithm>
#include <iterator>



#include "HttpRouting.h"
#include "HttpConnection.h"

std::unique_ptr<HttpRouting>  HttpRouting::singleton;

std::once_flag   HttpRouting::once;

bool HttpRouting::route(std::unique_ptr<HttpConnection> request) {
    bool handled = false;
    for(auto & route : routes) {
        if ( route.method == request->method ) {

            std::regex prefix(route.prefix);
            
            std::smatch matches;
            auto matched = std::regex_match(request->resource,matches,prefix);
            
            // std::cerr << route.prefix << "?" << request.resource  << "=" <<  matches.size() << std::endl;
            // There should always be two matches
            // element 0 is the complete string that was matched
            // everything after is all the submatches
            if ( matches.size() > 1 ) {
                // check to see if the used the whole string otherwise don't treat it as a match
                if (matches[0].str().size() == request->resource.size()) {
                    if ( handled ) {
                        std::cerr << "request already handled" << request->method << " " << request->resource << std::endl;
                        break;
                    }
                    handled = true;
                    route.route->handle(std::move(request),std::vector<std::string>(matches.begin()+1,matches.end()));
                    break;
                }
            }
            
            // handled = true;
        }
    }
    if ( !handled ) {
        request->set_status(HTTP_NOT_FOUND, "Not found");
        
        
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_response,request.release(),nullptr));
    
    }

    return handled;
}
