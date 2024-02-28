#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "../pilote_i2c/adxl345_ioctl.h"

int main(){
    int fdx = open("/dev/adxl345-0",O_RDONLY);
    int fdy = open("/dev/adxl345-0",O_RDONLY);
    int fdz = open("/dev/adxl345-0",O_RDONLY);
    if(fdx < 0 ||fdy < 0 ||fdz < 0 ){
        printf("Error in file open\n");
        return -1;
    }
    short x_axis,y_axis,z_axis;
    ioctl(fdx,READ_X);
    ioctl(fdy,READ_Y);
    ioctl(fdz,READ_Z);
    int read_cnt = read(fdx,(void*) &x_axis, 2);
    if(read_cnt != 2){
        printf("Error in read\n");
        return -1;
    }
    read_cnt = read(fdy,(void*) &y_axis, 2);
    if(read_cnt != 2){
        printf("Error in read\n");
        return -1;
    }
    read_cnt = read(fdz,(void*) &z_axis, 2);
    if(read_cnt != 2){
        printf("Error in read\n");
        return -1;
    }
    printf("X axis:%d\nY axis:%d\nZ axis:%d\n",x_axis,y_axis,z_axis);
    close(fdx);
    close(fdy);
    close(fdz);
    return 0;
}