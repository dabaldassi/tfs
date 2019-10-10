#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


int main(void)
{
  char buf[100] = "salut\n";
  int  fd = open("bar/file", O_RDWR);

  int w = write(fd, buf, 2);
  printf("%d w\n", w);
  w = write(fd,buf+2,2);
  close(fd);

  fd = open("bar/file", O_RDWR);
  bzero(buf,100);
  int r = read(fd, buf, 100);
  
  printf("%s %d\n", buf,r);

  close(fd);
  
  return 0;
}
