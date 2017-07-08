#include <stdio.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/mman.h>

int main() 
{
    int fd;
    fd = open("/home/hemanth/file_system/fs_checker/fs.img", O_RDONLY);
    if(fd == -1)
    {
        printf("ERROR: image not found \n");
        return 1;
    }

    struct stat s;
    int status = fstat (fd, & s);
    int size = s.st_size;

    char *content = (char*) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    return 0;
}
