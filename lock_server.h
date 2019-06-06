// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

enum class lock_state { FREE, LOCKED };

class lock_server {
private:
    std::mutex mutex_; // Protects locks_
    std::condition_variable release_cond_;
    std::unordered_map<lock_protocol::lockid_t, int> locks_;

protected:
    int nacquire;

public:
    lock_server();
    ~lock_server() {};
    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







