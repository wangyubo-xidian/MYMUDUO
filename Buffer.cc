#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

const char Buffer::kCRLF[] = "\r\n";

/**
 * 从fd上读取数据，Poller工作在LT模式
 * Buffer缓冲区有大小，但是从fd上读数据的时候却不知道tcp数据的最终大小 
 */
ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65536] = {0};   // 栈上的内存空间 64KB
    struct iovec vec[2];
    const size_t writable = writableBytes();   // Buffer缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n;
    }
    else   // extrabuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
         *savedErrno = errno;
    }
    return n;
}