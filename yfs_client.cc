// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
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
  lock_client_.reset(new lock_client(lock_dst));
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
  // You modify this function for Lab 3
  // - hold and release the file lock

  //lock_client_->acquire(inum);
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
  //lock_client_->release(inum);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

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
    return r;
}

int yfs_client::mkdir(inum parent, const std::string &name, inum& id) {
    int ret = create(parent, name, id, true);
    return ret;
}

int yfs_client::readdir(inum inum, std::list<dirent> &dirs) {
    //lock_client_->acquire(inum);
    std::map<std::string, unsigned long long> dirent;
    struct dirent dir;
    auto r = ec->readdir(inum, dirent);
    if (r != extent_protocol::OK) {
        goto release;
    }

    for (const auto &kv : dirent) {
        dir.name = kv.first;
        dir.inum = kv.second;
        dirs.emplace_back(std::move(dir));
    }

  release:
    //lock_client_->release(inum);
    return r;
}

int yfs_client::lookup(inum parent, const std::string &name, inum& id) {
    auto r = ec->lookup(parent, name, id);
    return r;
}

int yfs_client::read(inum id, std::size_t off, std::size_t size, std::string &buf) {
    std::string extent_buf;
    lock_client_->acquire(id);
    auto r = ec->get(id, extent_buf);
    if (r == OK) {
        buf = extent_buf.substr(off, size);
    }
    lock_client_->release(id);
    return r;
}

int yfs_client::write(inum id, std::size_t off, std::size_t size, const char *buf) {
    lock_client_->acquire(id);
    std::string extent_buf;
    auto r = ec->get(id, extent_buf);
    if (r != OK) {
        goto release;
    }

    printf("yfs_client before Write id %016llx buf_size %zu off %zu size %zu\n", id, extent_buf.size(), off, size);
    extent_buf.resize(std::max(extent_buf.size(), off + size), '\0');
    //extent_buf.replace(off, size, buf);
    for (std::size_t i = off, j = 0; j < size; ++i,++j){
         extent_buf[i] = buf[j];
    }

    r = ec->put(id, extent_buf);
    printf("yfs_client after Write id %016llx buf_size %zu\n", id, extent_buf.size());

  release:
    lock_client_->release(id);
    return r;
}

int yfs_client::setattr(inum ino, struct stat *st) {
    // Update mtime, atime
    lock_client_->acquire(ino);
    std::string tmp;
    auto ret = ec->get(ino, tmp);
    if (OK != ret){
        goto release;
    }
    if (st->st_size < (int)tmp.size()){
        tmp = tmp.substr(0, st->st_size);
    } else {
        tmp += std::string('\0', st->st_size - tmp.size());
    }
    ret = ec->put(ino, tmp);

release:
    lock_client_->release(ino);
    return ret;
}

int yfs_client::remove(inum parent, const std::string &name) {
    lock_client_->acquire(parent);
    inum id;
    int ret = lookup(parent, name, id);
    if (ret != OK) {
        goto release;
    }
    ret = ec->remove(id);

  release:
    lock_client_->release(parent);
    return ret;
}
