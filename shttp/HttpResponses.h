//
//  HttpResponses.h
//  shttp
//
//  Created by Mark Vicuna on 6/29/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpResponses_h
#define HttpResponses_h

#include <string>
//TODO: Unkludge response status messages
const std::string HTTP_ENDL = "\r\n";
const std::string HTTP_V1_0 = "HTTP/1.0";
const std::string HTTP_V1_1 = "HTTP/1.1";
const int HTTP_OK = 200;
const int  HTTP_NOT_FOUND = 404;
const int  HTTP_ERROR = 500;;
const std::string  HTTP_HEADER_CONTENT_TYPE = "Content-Type";
const std::string  HTTP_HEADER_CONTENT_LENGTH = "Content-Length";
const std::string  HTTP_HEADER_DATE = "Date";
const std::string  HTTP_HEADER_CONNECTION = "Connection";
const std::string  HTTP_HEADER_CONNECTION_CLOSE = "close";
const std::string  HTTP_HEADER_CONNECTION_KEEPALIVE = "keep-alive";
const std::string  HTTP_HEADER_END = HTTP_ENDL+HTTP_ENDL;


#endif /* HttpResponses_h */
