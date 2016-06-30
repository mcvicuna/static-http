//
//  HttpContentTypes.hpp
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpContentTypes_hpp
#define HttpContentTypes_hpp

#include <stdio.h>

#include <mutex>
#include <map>
#include <string>


class HttpContentTypes {
private:
    std::map<std::string,std::string> types;
    HttpContentTypes(std::map<std::string, std::string> _x);
    static std::unique_ptr<HttpContentTypes> singleton;
    static std::once_flag once;
public:
    static HttpContentTypes & get();
    std::string get_content_by_ext(const char *filename);
    std::string get_content_by_name(std::string key);
};
#endif /* HttpContentTypes_hpp */
