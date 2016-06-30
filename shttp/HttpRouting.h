//
//  HttpRouting.h
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpRouting_h
#define HttpRouting_h

#include <vector>
#include <mutex>
#include <regex>

#include "HttpRoute.h"
#include "HttpResponses.h"

class HttpConnection;

class HttpRouting {
    struct RouteConfig {
        std::string method;
        std::string prefix;
        std::unique_ptr<HttpRoute> route;
        RouteConfig(std::string _m, std::string _p, std::unique_ptr<HttpRoute> _r)
                    : method(_m), prefix(_p)
        {
            route = std::move(_r);
        };
    };
    
private:
    std::vector<RouteConfig> routes;
    std::unique_ptr<HttpRoute> default_route;
    HttpRouting() {};
    static std::unique_ptr<HttpRouting> singleton;
    static std::once_flag once;
public:
    
    void addRoute(std::string method, std::string prefix, std::unique_ptr<HttpRoute> route) {
        routes.push_back(RouteConfig(method,prefix,std::move(route)));
    }
    
    static HttpRouting & get() {
        std::call_once(HttpRouting::once,[]() {
            HttpRouting::singleton.reset(new HttpRouting());
        });
        return *singleton;
    }    
    
    bool route(std::unique_ptr<HttpConnection> request);
    
};



#endif /* HttpRouting_h */
