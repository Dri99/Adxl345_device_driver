#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(){
    int fd = open("/dev/adxl345-0",O_RDONLY);
    if(fd < 0){
        printf("Error in file open\n");
        return -1;
    }
    short x_axis;
    int read_cnt = read(fd,(void*) &x_axis, 2);
    if(read_cnt != 2){
        printf("Error in read\n");
        return -1;
    }
    printf("X axis:%d\n",x_axis);
    close(fd);
    return 0;
}