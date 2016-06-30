//
//  HttpContentTypes.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include "HttpContentTypes.h"


std::unique_ptr<HttpContentTypes >  HttpContentTypes::singleton;

std::once_flag   HttpContentTypes::once;

std::string HttpContentTypes::get_content_by_ext(const char *filename) {

    char *period = strrchr(filename, '.');
    std::string ext(period+1);
    
    return get_content_by_name(ext);
}

std::string HttpContentTypes::get_content_by_name(std::string key) {
    auto type = types.find(key);
    if ( type == types.end() ) {
        return std::string("application/stream");
    }    
    return type->second;
}

HttpContentTypes::HttpContentTypes( std::map<std::string, std::string> _x) : types(_x.begin(), _x.end())
{

}


HttpContentTypes & HttpContentTypes::get() {
    std::call_once(HttpContentTypes::once,[]() {
        // const std::map<std::string, std::string > x;
        //std::initializer_list<std::pair<std::string const,std::string const>> il
        const std::map<std::string, std::string > x = {
            {"jpg","image/jpeg"},
            {"html","text/html"},
            {"txt","text/text"},
            {"*","application/stream"},
        };
        HttpContentTypes::singleton.reset(new HttpContentTypes(x));
    });
    return *singleton;
}