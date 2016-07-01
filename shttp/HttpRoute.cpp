//
//  HttpRoute.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/30/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include <stdio.h>

#include "HttpPool.h"
#include "HttpRoute.h"
#include "HttpConnection.h"

bool HttpRoute::handle(std::unique_ptr<HttpConnection> request, std::vector<std::string> params) {
    bool retVal = do_handle(request,params);
    if ( retVal ) {
        HttpPool::get().enqueue(std::bind(&HttpConnection::write_response,request.release()));

    }
    return retVal;
};