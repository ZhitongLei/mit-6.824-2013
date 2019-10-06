// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <random>
#include <limits>

int random_id() {
    std::random_device r;
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(2, std::numeric_limits<int>::max());
    return uniform_dist(e1);
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int yfs_client::create(inum parent, const std::string &name, inum &id, bool is_dir) {
    // FIXME inum maybe duplicate
    //int max_tries = 3;
    int r;
    /*
    while (max_tries--) {
        id = random_id();
        if (is_dir) {
            id &= 0x7FFFFFFF;
        } else {
            id |= 0x80000000;
        }
        r = ec->create(parent, name, id);
        if (r == extent_protocol::EXIST || r == extent_protocol::OK) {
            break;
        }
    }*/
    id = random_id();
    if (is_dir) {
        id &= 0x7FFFFFFF;
    } else {
        id |= 0x80000000;
    }
    r = ec->create(parent, name, id);
    switch (r) {
      case extent_protocol::OK:
          return OK;
      case extent_protocol::EXIST:
          return EXIST;
      default:
          return IOERR;
    }

    return OK;
}

int yfs_client::mkdir(inum parent, const std::string &name, inum& id) {
    return create(parent, name, id, true);
}

int yfs_client::readdir(inum inum, std::list<dirent> &dirs) {
    std::map<std::string, unsigned long long> dirent;
    auto r = ec->readdir(inum, dirent);
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::NOENT:
          return NOENT;
      default:
          return IOERR;
    }

    struct dirent dir;
    for (const auto &kv : dirent) {
        dir.name = kv.first;
        dir.inum = kv.second;
        dirs.emplace_back(std::move(dir));
    }
    return OK;
}

int yfs_client::lookup(inum parent, const std::string &name, inum& id) {
    auto r = ec->lookup(parent, name, id);
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::NOENT:
          return NOENT;
      default:
          return IOERR;
    }
    return OK;
}

int yfs_client::read(inum id, std::size_t off, std::size_t size, std::string &buf) {
    std::string extent_buf;
    auto r = ec->get(id, extent_buf);
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::NOENT:
          return NOENT;
      default:
          return IOERR;
    }

    buf = extent_buf.substr(off, size);
    //std::size_t start = std::min(off, extent_buf.size());
    //std::size_t end = std::min(off+size, extent_buf.size());
    //std::size_t actual_size = end - start;
    //buf = extent_buf.substr(start, actual_size);
    return OK;
}

int yfs_client::write(inum id, std::size_t off, std::size_t size, const char *buf) {
    std::string extent_buf;
    auto r = ec->get(id, extent_buf);
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::NOENT:
          return NOENT;
      default:
          return IOERR;
    }

    //if (off > extent_buf.size()) {
    //    std::size_t gap = off - extent_buf.size();
    //    extent_buf.insert(extent_buf.end(), gap+size, '\0');
    //}
    printf("yfs_client before Write id %016llx buf_size %zu off %zu size %zu\n", id, extent_buf.size(), off, size);
    extent_buf.resize(std::max(extent_buf.size(), off + size), '\0');
    //extent_buf.replace(off, size, buf);
    for (std::size_t i = off, j = 0; j < size; ++i,++j){
         extent_buf[i] = buf[j];
    }

    r = ec->put(id, extent_buf);
    printf("yfs_client after Write id %016llx buf_size %zu\n", id, extent_buf.size());
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::NOENT:
          return NOENT;
      default:
          return IOERR;
    }

    return OK;
}

int yfs_client::setattr(inum ino, struct stat *st) {
    // Update mtime, atime
    std::string tmp;
    auto ret = ec->get(ino, tmp);
    if (OK != ret){
        return ret;
    }
    if (st->st_size < (int)tmp.size()){
        tmp = tmp.substr(0, st->st_size);
    } else {
        tmp += std::string('\0', st->st_size - tmp.size());
    }
    return ec->put(ino, tmp);
}

//int yfs_client::setattr(inum id, struct stat &st) {
//    int r = OK; 
//    extent_protocol::attr a;
//    if (ec->getattr(id, a) != extent_protocol::OK) {
//      r = IOERR;
//      goto release;
//    }
//
//release:
//    return r;
//}
