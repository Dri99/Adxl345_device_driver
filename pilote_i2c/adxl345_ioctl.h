#include <fcntl.h>
#include <sys/ioctl.h>

#define READ_X _IO(10, 1)
#define READ_Y _IO(10, 2)
#define READ_Z _IO(10, 3)