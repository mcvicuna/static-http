//
//  HttpPool.cpp
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#include "HttpPool.h"

#include <mutex>

std::unique_ptr<HttpPool>  HttpPool::singleton;

std::once_flag   HttpPool::once;
