//
//  HttpRoute.h
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpRoute_h
#define HttpRoute_h

class HttpConnection;

#include <memory>

class HttpRoute {
public:
    bool handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) {
        return do_handle(std::move(request),params);
    };

    
    virtual ~HttpRoute() = default;
private:
    virtual bool do_handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) = 0;
};


#endif /* HttpRoute_h */
