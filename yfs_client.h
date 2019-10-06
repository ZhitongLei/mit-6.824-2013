#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include <memory>
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"
#include "extent_client.h"
//#include "yfs_protocol.h"

class yfs_client {
  extent_client *ec;
  std::unique_ptr<lock_client> lock_client_;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int create(inum parent, const std::string &name, inum& id, bool is_dir = false);
  int mkdir(inum, const std::string &, inum&);
  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int readdir(inum, std::list<dirent> &);

  int read(inum, std::size_t off, std::size_t size, std::string &buf);
  int write(inum id, std::size_t off, std::size_t size, const char *buf);
  int lookup(inum, const std::string &, inum&);
  int setattr(inum ino, struct stat *st);
  int remove(inum, const std::string &);
};

#endif
