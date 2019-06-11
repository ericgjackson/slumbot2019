#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "io.h"

using std::string;
using std::vector;

// Note that stat() is very slow.  We've replaced the call to stat() with
// a call to open().
bool FileExists(const char *filename) {
  int fd = open(filename, O_RDONLY, 0);
  if (fd == -1) {
    if (errno == ENOENT) {
      // I believe this is what happens when there is no such file
      return false;
    }
    fprintf(stderr, "Failed to open \"%s\", errno %i\n", filename, errno);
    if (errno == 24) {
      fprintf(stderr, "errno 24 may indicate too many open files\n");
    }
    exit(-1);
  } else {
    close(fd);
    return true;
  }
#if 0
  struct stat stbuf;
  int ret = stat(filename, &stbuf);
  if (ret == -1) {
    return false;
  } else {
    return true;
  }
#endif
}

long long int FileSize(const char *filename) {
  struct stat stbuf;
  if (stat(filename, &stbuf) == -1) {
    fprintf(stderr, "FileSize: Couldn't access: %s\n", filename);
    exit(-1);
  }
  return stbuf.st_size;
}

void Reader::OpenFile(const char *filename) {
  filename_ = filename;
  struct stat stbuf;
  if (stat(filename, &stbuf) == -1) {
    fprintf(stderr, "Reader::OpenFile: Couldn't access: %s\n", filename);
    exit(-1);
  }
  file_size_ = stbuf.st_size;
  remaining_ = file_size_;

  fd_ = open(filename, O_RDONLY, 0);
  if (fd_ == -1) {
    fprintf(stderr, "Failed to open \"%s\", errno %i\n", filename, errno);
    if (errno == 24) {
      fprintf(stderr, "errno 24 may indicate too many open files\n");
    }
    exit(-1);
  }

  overflow_size_ = 0;
  byte_pos_ = 0;
}

Reader::Reader(const char *filename) {
  OpenFile(filename);

  buf_size_ = kBufSize;
  if (remaining_ < buf_size_) buf_size_ = remaining_;

  buf_.reset(new unsigned char[buf_size_]);
  buf_ptr_ = buf_.get();
  end_read_ = buf_.get();

  if (! Refresh()) {
    fprintf(stderr, "Warning: empty file: %s\n", filename);
  }
}

// This constructor for use by NewReaderMaybe().  Doesn't call stat().
// I should clean up this code to avoid redundancy.
Reader::Reader(const char *filename, long long int file_size) {
  filename_ = filename;
  file_size_ = file_size;
  remaining_ = file_size;

  fd_ = open(filename, O_RDONLY, 0);
  if (fd_ == -1) {
    fprintf(stderr, "Failed to open \"%s\", errno %i\n", filename, errno);
    if (errno == 24) {
      fprintf(stderr, "errno 24 may indicate too many open files\n");
    }
    exit(-1);
  }

  overflow_size_ = 0;
  byte_pos_ = 0;

  buf_size_ = kBufSize;
  if (remaining_ < buf_size_) buf_size_ = remaining_;

  buf_.reset(new unsigned char[buf_size_]);
  buf_ptr_ = buf_.get();
  end_read_ = buf_.get();

  if (! Refresh()) {
    fprintf(stderr, "Warning: empty file: %s\n", filename);
  }
}

// Returns NULL if no file by this name exists
Reader *NewReaderMaybe(const char *filename) {
  struct stat stbuf;
  if (stat(filename, &stbuf) == -1) {
    return NULL;
  }
  long long int file_size = stbuf.st_size;
  return new Reader(filename, file_size);
}

Reader::~Reader(void) {
  close(fd_);
}

bool Reader::AtEnd(void) const {
  // This doesn't work for CompressedReader
  // return byte_pos_ == file_size_;
  return (buf_ptr_ == end_read_ && remaining_ == 0 && overflow_size_ == 0);
}

void Reader::SeekTo(long long int offset) {
  long long int ret = lseek(fd_, offset, SEEK_SET);
  if (ret == -1) {
    fprintf(stderr, "lseek failed, offset %lli, ret %lli, errno %i, fd %i\n",
	    offset, ret, errno, fd_);
    fprintf(stderr, "File: %s\n", filename_.c_str());
    exit(-1);
  }
  remaining_ = file_size_ - offset;
  overflow_size_ = 0;
  byte_pos_ = offset;
  Refresh();
}

bool Reader::Refresh(void) {
  if (remaining_ == 0 && overflow_size_ == 0) return false;

  if (overflow_size_ > 0) {
    memcpy(buf_.get(), overflow_, overflow_size_);
  }
  buf_ptr_ = buf_.get();
    
  unsigned char *read_into = buf_.get() + overflow_size_;

  int to_read = buf_size_ - overflow_size_;
  if (to_read > remaining_) to_read = remaining_;

  int ret;
  if ((ret = read(fd_, read_into, to_read)) != to_read) {
    fprintf(stderr, "Read returned %i not %i\n", ret, to_read);
    fprintf(stderr, "File: %s\n", filename_.c_str());
    fprintf(stderr, "remaining_ %lli\n", remaining_);
    exit(-1);
  }

  remaining_ -= to_read;
  end_read_ = read_into + to_read;
  overflow_size_ = 0;

  return true;
}

bool Reader::GetLine(string *s) {
  s->clear();
  while (true) {
    if (buf_ptr_ == end_read_) {
      if (! Refresh()) {
	return false;
      }
    }
    if (*buf_ptr_ == '\r') {
      ++buf_ptr_;
      ++byte_pos_;
      continue;
    }
    if (*buf_ptr_ == '\n') {
      ++buf_ptr_;
      ++byte_pos_;
      break;
    }
    s->push_back(*buf_ptr_);
    ++buf_ptr_;
    ++byte_pos_;
  }
  return true;
}

bool Reader::ReadInt(int *i) {
  if (buf_ptr_ + sizeof(int) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  char my_buf[4];
  my_buf[0] = *buf_ptr_++;
  my_buf[1] = *buf_ptr_++;
  my_buf[2] = *buf_ptr_++;
  my_buf[3] = *buf_ptr_++;
  byte_pos_ += 4;
  int *int_ptr = reinterpret_cast<int *>(my_buf);
  *i = *int_ptr;
  return true;
}

int Reader::ReadIntOrDie(void) {
  int i;
  if (! ReadInt(&i)) {
    fprintf(stderr, "Couldn't read int; file %s byte pos %lli\n",
	    filename_.c_str(), byte_pos_);
    exit(-1);
  }
  return i;
}

bool Reader::ReadUnsignedInt(unsigned int *u) {
  if (buf_ptr_ + sizeof(int) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  char my_buf[4];
  my_buf[0] = *buf_ptr_++;
  my_buf[1] = *buf_ptr_++;
  my_buf[2] = *buf_ptr_++;
  my_buf[3] = *buf_ptr_++;
  byte_pos_ += 4;
  unsigned int *u_int_ptr = reinterpret_cast<unsigned int *>(my_buf);
  *u = *u_int_ptr;
  return true;
}

unsigned int Reader::ReadUnsignedIntOrDie(void) {
  unsigned int u;
  if (! ReadUnsignedInt(&u)) {
    fprintf(stderr, "Couldn't read unsigned int\n");
    fprintf(stderr, "File: %s\n", filename_.c_str());
    fprintf(stderr, "Byte pos: %lli\n", byte_pos_);
    exit(-1);
  }
  return u;
}

bool Reader::ReadLong(long long int *l) {
  if (buf_ptr_ + sizeof(long long int) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *l = *(long long int *)buf_ptr_;
  buf_ptr_ += sizeof(long long int);
  byte_pos_ += sizeof(long long int);
  return true;
}

long long int Reader::ReadLongOrDie(void) {
  long long int l;
  if (! ReadLong(&l)) {
    fprintf(stderr, "Couldn't read long\n");
    exit(-1);
  }
  return l;
}

bool Reader::ReadUnsignedLong(unsigned long long int *u) {
  if (buf_ptr_ + sizeof(unsigned long long int) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *u = *(unsigned long long int *)buf_ptr_;
  buf_ptr_ += sizeof(unsigned long long int);
  byte_pos_ += sizeof(unsigned long long int);
  return true;
}

unsigned long long int Reader::ReadUnsignedLongOrDie(void) {
  unsigned long long int u;
  if (! ReadUnsignedLong(&u)) {
    fprintf(stderr, "Couldn't read unsigned long\n");
    exit(-1);
  }
  return u;
}

bool Reader::ReadShort(short *s) {
  if (buf_ptr_ + sizeof(short) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  // Possible alignment issue?
  *s = *(short *)buf_ptr_;
  buf_ptr_ += sizeof(short);
  byte_pos_ += sizeof(short);
  return true;
}

short Reader::ReadShortOrDie(void) {
  short s;
  if (! ReadShort(&s)) {
    fprintf(stderr, "Couldn't read short\n");
    exit(-1);
  }
  return s;
}

bool Reader::ReadUnsignedShort(unsigned short *u) {
  if (buf_ptr_ + sizeof(unsigned short) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *u = *(unsigned short *)buf_ptr_;
  buf_ptr_ += sizeof(unsigned short);
  byte_pos_ += sizeof(unsigned short);
  return true;
}

unsigned short Reader::ReadUnsignedShortOrDie(void) {
  unsigned short s;
  if (! ReadUnsignedShort(&s)) {
    fprintf(stderr, "Couldn't read unsigned short; file %s byte pos %lli "
	    "file_size %lli\n", filename_.c_str(), byte_pos_, file_size_);
    exit(-1);
  }
  return s;
}

bool Reader::ReadChar(char *c) {
  if (buf_ptr_ + sizeof(char) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *c = *(char *)buf_ptr_;
  buf_ptr_ += sizeof(char);
  byte_pos_ += sizeof(char);
  return true;
}

char Reader::ReadCharOrDie(void) {
  char c;
  if (! ReadChar(&c)) {
    fprintf(stderr, "Couldn't read char\n");
    exit(-1);
  }
  return c;
}

bool Reader::ReadUnsignedChar(unsigned char *u) {
  if (buf_ptr_ + sizeof(unsigned char) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *u = *(unsigned char *)buf_ptr_;
  buf_ptr_ += sizeof(unsigned char);
  byte_pos_ += sizeof(unsigned char);
  return true;
}

unsigned char Reader::ReadUnsignedCharOrDie(void) {
  unsigned char u;
  if (! ReadUnsignedChar(&u)) {
    fprintf(stderr, "Couldn't read unsigned char\n");
    fprintf(stderr, "File: %s\n", filename_.c_str());
    fprintf(stderr, "Byte pos: %lli\n", byte_pos_);
    exit(-1);
  }
  return u;
}

void Reader::ReadOrDie(unsigned char *c) {
  *c = ReadUnsignedCharOrDie();
}

void Reader::ReadOrDie(unsigned short *s) {
  *s = ReadUnsignedShortOrDie();
}

void Reader::ReadOrDie(unsigned int *u) {
  *u = ReadUnsignedIntOrDie();
}

void Reader::ReadOrDie(int *i) {
  *i = ReadIntOrDie();
}

void Reader::ReadOrDie(double *d) {
  *d = ReadDoubleOrDie();
}

void Reader::ReadNBytesOrDie(unsigned int num_bytes, unsigned char *buf) {
  for (unsigned int i = 0; i < num_bytes; ++i) {
    if (buf_ptr_ + 1 > end_read_) {
      if (! Refresh()) {
	fprintf(stderr, "Couldn't read %i bytes\n", num_bytes);
	fprintf(stderr, "Filename: %s\n", filename_.c_str());
	fprintf(stderr, "File size: %lli\n", file_size_);
	fprintf(stderr, "Before read byte pos: %lli\n", byte_pos_);
	fprintf(stderr, "Overflow size: %i\n", overflow_size_);
	fprintf(stderr, "i %i\n", i);
	exit(-1);
      }
    }
    buf[i] = *buf_ptr_++;
    ++byte_pos_;
  }
}

void Reader::ReadEverythingLeft(unsigned char *data) {
  unsigned long long int data_pos = 0ULL;
  unsigned long long int left = file_size_ - byte_pos_;
  while (left > 0) {
    unsigned long long int num_bytes = end_read_ - buf_ptr_;
    memcpy(data + data_pos, buf_ptr_, num_bytes);
    buf_ptr_ = end_read_;
    data_pos += num_bytes;
    if (data_pos > left) {
      fprintf(stderr, "ReadEverythingLeft: read too much?!?\n");
      exit(-1);
    } else if (data_pos == left) {
      break;
    }
    if (! Refresh()) {
      fprintf(stderr, "ReadEverythingLeft: premature EOF?!?\n");
      exit(-1);
    }
  }
}

bool Reader::ReadCString(string *s) {
  *s = "";
  while (true) {
    if (buf_ptr_ + 1 > end_read_) {
      if (! Refresh()) {
	return false;
      }
    }
    char c = *buf_ptr_++;
    ++byte_pos_;
    if (c == 0) return true;
    *s += c;
  }
}

string Reader::ReadCStringOrDie(void) {
  string s;
  if (! ReadCString(&s)) {
    fprintf(stderr, "Couldn't read string\n");
    exit(-1);
  }
  return s;
}

bool Reader::ReadDouble(double *d) {
  if (buf_ptr_ + sizeof(double) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *d = *(double *)buf_ptr_;
  buf_ptr_ += sizeof(double);
  byte_pos_ += sizeof(double);
  return true;
}

double Reader::ReadDoubleOrDie(void) {
  double d;
  if (! ReadDouble(&d)) {
    fprintf(stderr, "Couldn't read double: file %s byte pos %lli\n",
	    filename_.c_str(), byte_pos_);
    exit(-1);
  }
  return d;
}

bool Reader::ReadFloat(float *f) {
  if (buf_ptr_ + sizeof(float) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *f = *(float *)buf_ptr_;
  buf_ptr_ += sizeof(float);
  byte_pos_ += sizeof(float);
  return true;
}

float Reader::ReadFloatOrDie(void) {
  float f;
  if (! ReadFloat(&f)) {
    fprintf(stderr, "Couldn't read float: file %s\n", filename_.c_str());
    exit(-1);
  }
  return f;
}

// Identical to ReadDouble()
bool Reader::ReadReal(double *d) {
  if (buf_ptr_ + sizeof(double) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *d = *(double *)buf_ptr_;
  buf_ptr_ += sizeof(double);
  byte_pos_ += sizeof(double);
  return true;
}

// Identical to ReadFloat()
bool Reader::ReadReal(float *f) {
  if (buf_ptr_ + sizeof(float) > end_read_) {
    if (buf_ptr_ < end_read_) {
      overflow_size_ = (int)(end_read_ - buf_ptr_);
      memcpy(overflow_, buf_ptr_, overflow_size_);
    }
    if (! Refresh()) {
      return false;
    }
  }
  *f = *(float *)buf_ptr_;
  buf_ptr_ += sizeof(float);
  byte_pos_ += sizeof(float);
  return true;
}

void Writer::Init(const char *filename, bool modify, int buf_size) {
  filename_ = filename;
  if (modify) {
    fd_ = open(filename, O_WRONLY, 0666);
    if (fd_ < 0 && errno == ENOENT) {
      // If file doesn't exist, open it with creat()
      fd_ = creat(filename, 0666);
    }
  } else {
    // creat() is supposedly equivalent to passing
    // O_WRONLY|O_CREAT|O_TRUNC to open().
    fd_ = creat(filename, 0666);
  }
  if (fd_ < 0) {
    // Is this how errors are indicated?
    fprintf(stderr, "Couldn't open %s for writing (errno %i)\n", filename,
	    errno);
    exit(-1);
  }

  buf_size_ = buf_size;
  buf_.reset(new unsigned char[buf_size_]);
  end_buf_ = buf_.get() + buf_size_;
  buf_ptr_ = buf_.get();
}

Writer::Writer(const char *filename, int buf_size) {
  Init(filename, false, buf_size);
}

Writer::Writer(const char *filename, bool modify) {
  Init(filename, modify, kBufSize);
}

Writer::Writer(const char *filename) {
  Init(filename, false, kBufSize);
}

Writer::~Writer(void) {
  Flush();
  close(fd_);
}

// Generally write() writes everything in one call.  Haven't seen any cases
// that justify the loop I do below.  Could take it out.
void Writer::Flush(void) {
  if (buf_ptr_ > buf_.get()) {
    int left_to_write = (int)(buf_ptr_ - buf_.get());
    while (left_to_write > 0) {
      int written = write(fd_, buf_.get(), left_to_write);
      if (written < 0) {
	fprintf(stderr,
		"Error in flush: tried to write %i, return of %i; errno %i; "
		"fd %i\n", left_to_write, written, errno, fd_);
	exit(-1);
      } else if (written == 0) {
	// Stall for a bit to avoid busy loop
	sleep(1);
      }
      left_to_write -= written;
    }
  }
  buf_ptr_ = buf_.get();
}

// Only makes sense to call if we created the Writer with modify=true
void Writer::SeekTo(long long int offset) {
  Flush();
  long long int ret = lseek(fd_, offset, SEEK_SET);
  if (ret == -1) {
    fprintf(stderr, "lseek failed, offset %lli, ret %lli, errno %i, fd %i\n",
	    offset, ret, errno, fd_);
    fprintf(stderr, "File: %s\n", filename_.c_str());
    exit(-1);
  }
}

long long int Writer::Tell(void) {
  Flush();
  return lseek(fd_, 0LL, SEEK_CUR);
}

void Writer::WriteInt(int i) {
  if (buf_ptr_ + sizeof(int) > end_buf_) {
    Flush();
  }
  // Couldn't we have an alignment issue if we write a char and then an int,
  // for example?
  // *(int *)buf_ptr_ = i;
  memcpy(buf_ptr_, (void *)&i, sizeof(int));
  buf_ptr_ += sizeof(int);
}

void Writer::WriteUnsignedInt(unsigned int u) {
  if (buf_ptr_ + sizeof(unsigned int) > end_buf_) {
    Flush();
  }
  *(unsigned int *)buf_ptr_ = u;
  buf_ptr_ += sizeof(unsigned int);
}

void Writer::WriteLong(long long int l) {
  if (buf_ptr_ + sizeof(long long int) > end_buf_) {
    Flush();
  }
  *(long long int *)buf_ptr_ = l;
  buf_ptr_ += sizeof(long long int);
}

void Writer::WriteUnsignedLong(unsigned long long int u) {
  if (buf_ptr_ + sizeof(unsigned long long int) > end_buf_) {
    Flush();
  }
  *(unsigned long long int *)buf_ptr_ = u;
  buf_ptr_ += sizeof(unsigned long long int);
}

void Writer::WriteShort(short s) {
  if (buf_ptr_ + sizeof(short) > end_buf_) {
    Flush();
  }
  *(short *)buf_ptr_ = s;
  buf_ptr_ += sizeof(short);
}

void Writer::WriteChar(char c) {
  if (buf_ptr_ + sizeof(char) > end_buf_) {
    Flush();
  }
  *(char *)buf_ptr_ = c;
  buf_ptr_ += sizeof(char);
}

void Writer::WriteUnsignedChar(unsigned char c) {
  if (buf_ptr_ + sizeof(unsigned char) > end_buf_) {
    Flush();
  }
  *(unsigned char *)buf_ptr_ = c;
  buf_ptr_ += sizeof(unsigned char);
}

void Writer::WriteUnsignedShort(unsigned short s) {
  if (buf_ptr_ + sizeof(unsigned short) > end_buf_) {
    Flush();
  }
  *(unsigned short *)buf_ptr_ = s;
  buf_ptr_ += sizeof(unsigned short);
}

void Writer::WriteFloat(float f) {
  if (buf_ptr_ + sizeof(float) > end_buf_) {
    Flush();
  }
  *(float *)buf_ptr_ = f;
  buf_ptr_ += sizeof(float);
}

void Writer::WriteDouble(double d) {
  if (buf_ptr_ + sizeof(double) > end_buf_) {
    Flush();
  }
  *(double *)buf_ptr_ = d;
  buf_ptr_ += sizeof(double);
}

// Identical to WriteFloat()
void Writer::WriteReal(float f) {
  if (buf_ptr_ + sizeof(float) > end_buf_) {
    Flush();
  }
  *(float *)buf_ptr_ = f;
  buf_ptr_ += sizeof(float);
}

// Identical to WriteDouble()
void Writer::WriteReal(double d) {
  if (buf_ptr_ + sizeof(double) > end_buf_) {
    Flush();
  }
  *(double *)buf_ptr_ = d;
  buf_ptr_ += sizeof(double);
}

void Writer::Write(unsigned char c) {
  WriteUnsignedChar(c);
}

void Writer::Write(unsigned short s) {
  WriteUnsignedShort(s);
}

void Writer::Write(int i) {
  WriteInt(i);
}

void Writer::Write(unsigned int u) {
  WriteUnsignedInt(u);
}

void Writer::Write(double d) {
  WriteDouble(d);
}

void Writer::WriteCString(const char *s) {
  int len = strlen(s);
  if (buf_ptr_ + len + 1 > end_buf_) {
    Flush();
  }
  memcpy(buf_ptr_, s, len);
  buf_ptr_[len] = 0;
  buf_ptr_ += len + 1;
}

// Does not write num_bytes into file
void Writer::WriteNBytes(unsigned char *bytes, unsigned int num_bytes) {
  if ((int)num_bytes > buf_size_) {
    Flush();
    while ((int)num_bytes > buf_size_) {
      buf_size_ *= 2;
      buf_.reset(new unsigned char[buf_size_]);
      buf_ptr_ = buf_.get();
      end_buf_ = buf_.get() + buf_size_;
    }
  }
  if (buf_ptr_ + num_bytes > end_buf_) {
    Flush();
  }
  memcpy(buf_ptr_, bytes, num_bytes);
  buf_ptr_ += num_bytes;
}

void Writer::WriteBytes(unsigned char *bytes, int num_bytes) {
  WriteInt(num_bytes);
  if (num_bytes > buf_size_) {
    Flush();
    while (num_bytes > buf_size_) {
      buf_size_ *= 2;
      buf_.reset(new unsigned char[buf_size_]);
      buf_ptr_ = buf_.get();
      end_buf_ = buf_.get() + buf_size_;
    }
  }
  if (buf_ptr_ + num_bytes > end_buf_) {
    Flush();
  }
  memcpy(buf_ptr_, bytes, num_bytes);
  buf_ptr_ += num_bytes;
}

void Writer::WriteText(const char *s) {
  int len = strlen(s);
  if (buf_ptr_ + len + 1 > end_buf_) {
    Flush();
  }
  memcpy(buf_ptr_, s, len);
  buf_ptr_ += len;
}

int Writer::BufPos(void) {
  return (int)(buf_ptr_ - buf_.get());
}

ReadWriter::ReadWriter(const char *filename) {
  filename_ = filename;
  fd_ = open(filename, O_RDWR, 0666);
  if (fd_ < 0 && errno == ENOENT) {
    fprintf(stderr, "Can only create a ReadWriter on an existing file\n");
    fprintf(stderr, "Filename: %s\n", filename);
    exit(-1);
  }
}

ReadWriter::~ReadWriter(void) {
  close(fd_);
}

void ReadWriter::SeekTo(long long int offset) {
  long long int ret = lseek(fd_, offset, SEEK_SET);
  if (ret == -1) {
    fprintf(stderr, "lseek failed, offset %lli, ret %lli, errno %i, fd %i\n",
	    offset, ret, errno, fd_);
    fprintf(stderr, "File: %s\n", filename_.c_str());
    exit(-1);
  }
}

int ReadWriter::ReadIntOrDie(void) {
  int i, ret;
  if ((ret = read(fd_, &i, 4)) != 4) {
    fprintf(stderr, "ReadWriter::ReadInt returned %i not 4\n", ret);
    fprintf(stderr, "File: %s\n", filename_.c_str());
    exit(-1);
  }
  return i;
}

void ReadWriter::WriteInt(int i) {
  int written = write(fd_, &i, 4);
  if (written != 4) {
    fprintf(stderr, "Error: tried to write 4 bytes, return of %i; fd %i\n",
	    written, fd_);
    exit(-1);
  }
}

bool IsADirectory(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) != 0) return 0;
  return S_ISDIR(statbuf.st_mode);
}

// Filenames in listing returned are full paths
void GetDirectoryListing(const char *dir, vector<string> *listing) {
  int dirlen = strlen(dir);
  bool ends_in_slash = (dir[dirlen - 1] == '/');
  listing->clear();
  DIR *dfd = opendir(dir);
  if (dfd == NULL) {
    fprintf(stderr, "GetDirectoryListing: could not open directory %s\n", dir);
    exit(-1);
  }
  dirent *dp;
  while ((dp = readdir(dfd))) {
    if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
      string full_path = dir;
      if (! ends_in_slash) {
	full_path += "/";
      }
      full_path += dp->d_name;
      listing->push_back(full_path);
    }
  }
  closedir(dfd);
}

// Can handle files or directories
static void RecursivelyDelete(const string &path) {
  if (! IsADirectory(path.c_str())) {
    // fprintf(stderr, "Removing file %s\n", path.c_str());
    RemoveFile(path.c_str());
    return;
  }
  vector<string> listing;
  GetDirectoryListing(path.c_str(), &listing);
  unsigned int num = listing.size();
  for (unsigned int i = 0; i < num; ++i) {
    RecursivelyDelete(listing[i]);
  }
  // fprintf(stderr, "Removing dir %s\n", path.c_str());
  RemoveFile(path.c_str());
}

void RecursivelyDeleteDirectory(const char *dir) {
  // Succeed silently if directory doesn't exist
  if (! FileExists(dir)) return;
  if (! IsADirectory(dir)) {
    fprintf(stderr, "Path supplied is not a directory: %s\n", dir);
    return;
  }
  vector<string> listing;
  GetDirectoryListing(dir, &listing);
  unsigned int num = listing.size();
  for (unsigned int i = 0; i < num; ++i) {
    RecursivelyDelete(listing[i]);
  }
  // fprintf(stderr, "Removing dir %s\n", dir);
  RemoveFile(dir);
}

// Gives read/write/execute permissions to everyone
void Mkdir(const char *dir) {
  int ret = mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
  if (ret != 0) {
    if (errno == 17) {
      // File or directory by this name already exists.  We'll just assume
      // it's a directory and return successfully.
      return;
    }
    fprintf(stderr, "mkdir returned %i; errno %i\n", ret, errno);
    fprintf(stderr, "Directory: %s\n", dir);
    exit(-1);
  }
}

// FileExists() calls stat() which is very expensive.  Try calling
// remove() without a preceding FileExists() check.
void RemoveFile(const char *filename) {
  int ret = remove(filename);
  if (ret) {
    // ENOENT just signifies that there is no file by the given name
    if (errno != ENOENT) {
      fprintf(stderr, "Error removing file: %i; errno %i\n", ret, errno);
      exit(-1);
    }
  }
}

// How is this different from RemoveFile()?
void UnlinkFile(const char *filename) {
  int ret = unlink(filename);
  if (ret) {
    // ENOENT just signifies that there is no file by the given name
    if (errno != ENOENT) {
      fprintf(stderr, "Error unlinking file: %i; errno %i\n", ret, errno);
      exit(-1);
    }
  }
}

void MoveFile(const char *old_location, const char *new_location) {
  if (! FileExists(old_location)) {
    fprintf(stderr, "MoveFile: old location \"%s\" does not exist\n",
	    old_location);
    exit(-1);
  }
  if (FileExists(new_location)) {
    fprintf(stderr, "MoveFile: new location \"%s\" already exists\n",
	    new_location);
    exit(-1);
  }
  int ret = rename(old_location, new_location);
  if (ret != 0) {
    fprintf(stderr, "MoveFile: rename() returned %i\n", ret);
    fprintf(stderr, "Old location: %s\n", old_location);
    fprintf(stderr, "New location: %s\n", new_location);
    exit(-1);
  }
}

void CopyFile(const char *old_location, const char *new_location) {
  Reader reader(old_location);
  Writer writer(new_location);
  unsigned char uc;
  while (reader.ReadUnsignedChar(&uc)) {
    writer.WriteUnsignedChar(uc);
  }
}

