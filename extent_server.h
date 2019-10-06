// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "extent_protocol.h"

struct DirEntent {
    DirEntent(extent_protocol::extentid_t eid, const std::string &n)
        : id(eid),
          name(n) {
    }

    extent_protocol::extentid_t id;
    std::string name;
};

struct ExtentEntry {
    ExtentEntry() = default;

    ExtentEntry(extent_protocol::extentid_t id, extent_protocol::extentid_t parent_id, const std::string &name)
        : id_(id),
          parent_id_(parent_id),
          name_(name) {
        attr_.atime = attr_.ctime = attr_.mtime = std::time(nullptr);
        attr_.size = 0;
    }

    ExtentEntry(ExtentEntry &&rhs) 
        : id_(rhs.id_),
          parent_id_(rhs.parent_id_),
          name_(std::move(rhs.name_)),
          attr_(rhs.attr_),
          children_(std::move(rhs.children_)),
          buffer_(std::move(rhs.buffer_)) {
        rhs.id_ = 0;
        rhs.parent_id_ = 0;
    }

    ExtentEntry& operator=(ExtentEntry &&rhs) {
        if (&rhs != this) {
            id_ = rhs.id_;
            parent_id_ = rhs.parent_id_;
            name_ = std::move(rhs.name_);
            attr_ = rhs.attr_;
            children_ = std::move(rhs.children_);
            buffer_ = std::move(rhs.buffer_);
            rhs.id_ = 0;
            rhs.parent_id_ = 0;
        }
        return *this;
    }

    extent_protocol::extentid_t id_ = 0;
    extent_protocol::extentid_t parent_id_ = 0;
    std::string name_;
    extent_protocol::attr attr_{0, 0, 0, 0};

    std::list<DirEntent> children_;
    std::string buffer_;
};

class extent_server {

 public:
  extent_server();

  int create(extent_protocol::extentid_t parent_id, std::string name, extent_protocol::extentid_t id, int &);
  int lookup(extent_protocol::extentid_t parent_id, std::string name, extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

  int readdir(extent_protocol::extentid_t id, std::map<std::string, unsigned long long> &dirent);

private:
    bool isfile(extent_protocol::extentid_t id) {
        return (id & 0x80000000);
    }

private:
    std::mutex mutex_;  // Protects extents_
    std::unordered_map<extent_protocol::extentid_t, ExtentEntry> extents_;
};

#endif 







