// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <ctime>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <random>
#include <tuple>
#include <algorithm>

const extent_protocol::extentid_t kRootID = 0x00000001;

extent_server::extent_server() {
    ExtentEntry ext(kRootID, 0, "");
    extents_.emplace(kRootID, std::move(ext));
}

int extent_server::create(extent_protocol::extentid_t parent_id, std::string name, extent_protocol::extentid_t id, int &) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = extents_.find(parent_id);
    if (it == extents_.end() || isfile(parent_id)) {
        return extent_protocol::IOERR;
    }

    auto &parent_ext = it->second;
    const auto cit = std::find_if(parent_ext.children_.begin(), parent_ext.children_.end(),
                                  [&name](const DirEntent &dirent) {return dirent.name == name;});
    if (cit != parent_ext.children_.end()) {
        return extent_protocol::EXIST;
    }

    // XXX id mayby duplicated
    parent_ext.children_.emplace_back(id, name);
    extents_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(id),
                     std::forward_as_tuple(id, parent_id, name));
    parent_ext.attr_.mtime = parent_ext.attr_.ctime = std::time(nullptr);
    return extent_protocol::OK;
}

int extent_server::lookup(extent_protocol::extentid_t parent_id, std::string name, extent_protocol::extentid_t &id) {
    std::map<std::string, unsigned long long> dirent;
    int ret = readdir(parent_id, dirent);
    if (ret != extent_protocol::OK) {
        return ret;
    }

    const auto it = dirent.find(name);
    if (it == dirent.end()) {
        return extent_protocol::NOENT;
    }
    id = it->second;
    return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &ret)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = extents_.find(id);
  if (it != extents_.end()) {
      auto &ext = it->second;
      ext.buffer_.assign(buf.begin(), buf.end());
      ext.attr_.size = buf.size();
      ext.attr_.mtime = ext.attr_.atime = std::time(nullptr);
      ret = ext.buffer_.size();
      return extent_protocol::OK;
  }
  return extent_protocol::NOENT;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = extents_.find(id);
  if (it == extents_.end()) {
      return extent_protocol::NOENT;
  }
  auto &ext = it->second;
  buf.assign(ext.buffer_.begin(), ext.buffer_.end());
  ext.attr_.atime = std::time(nullptr);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = extents_.find(id);
  if (it == extents_.end()) {
      return extent_protocol::NOENT;
  }
  const auto &ext = it->second;
  a = ext.attr_;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = extents_.find(id);
  if (it == extents_.end()) {
      return extent_protocol::NOENT;
  }

  // TODO remove subdir recursively
  auto &ext = it->second;
  auto pit = extents_.find(ext.parent_id_);
  if (pit == extents_.end()) {
      return extent_protocol::IOERR;
  }

  pit->second.children_.remove_if([id](const DirEntent &dirent) {return dirent.id == id;});

  for (const auto &child : ext.children_) {
      extents_.erase(child.id);
  }

  pit->second.attr_.mtime = pit->second.attr_.ctime = std::time(nullptr);
  extents_.erase(id);
  return extent_protocol::OK;
}

int extent_server::readdir(extent_protocol::extentid_t id,
                           std::map<std::string, unsigned long long> &dirent) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = extents_.find(id);
  if (it == extents_.end() || isfile(id)) {
      return extent_protocol::NOENT;
  }
  auto &ext = it->second;
  for (const auto &child : ext.children_) {
      dirent.emplace(child.name, child.id);
  }
  return extent_protocol::OK;
}
