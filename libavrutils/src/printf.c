
#include "serial.h"
#include <stdio.h>
static int ioPutc(char c, FILE* stream)
{
  serial_putch(c);
  return 0;
}

static int ioGetc(FILE*stream)
{
  return serial_getch();
}

void startIO()
{
    static FILE myfd;
    fdev_setup_stream(&myfd,ioPutc,ioGetc,_FDEV_SETUP_RW);
    stdout=&myfd;
    stdin=&myfd;
    stderr=&myfd;
}