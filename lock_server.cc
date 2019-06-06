// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &) {
    lock_protocol::status ret = lock_protocol::OK;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        release_cond_.wait(lock, [this, lid]{return locks_.count(lid) == 0;});
        locks_.emplace(lid, clt);
        //break;
    }
    return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &) {
    lock_protocol::status ret = lock_protocol::OK;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = locks_.find(lid);
        if (it != locks_.end() && it->second == clt) {
            locks_.erase(it);
            release_cond_.notify_all();
        }
    }
    return ret;
}


