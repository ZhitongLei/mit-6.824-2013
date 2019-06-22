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

int yfs_client::create(inum parent, const std::string &name, inum &inum) {
    auto r = ec->create(inum, name, inum);
    switch (r) {
      case extent_protocol::OK:
          break;
      case extent_protocol::EXIST:
          return EXIST;
      default:
          return IOERR;
    }
    return OK;
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

    std::size_t actual_off = std::min(off, extent_buf.size());
    buf = extent_buf.substr(actual_off, size);
    return OK;
}
