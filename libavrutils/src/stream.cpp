#include "stream.h"
#include <stdio.h>
#include "serial.h"



Stream::Stream(_p_read r,_p_write w,_p_available a)
{
	_read = r;
	_write = w;
	_available = a;
	
	
}

  void Stream::print(const char * data){
	  printf("%s\n",data);
	  }
  void Stream::print(uint16_t data)
  {
	  printf("%u",data);
	  }
  void Stream::print(uint32_t data, int data2)
  {
	  printf("%lu %u ",data,data2);
	  }
  void Stream::println(){printf("\n");
  }
  void Stream::println(uint32_t data){printf("%lu\n",data);
  }
  void Stream::println(const char * data){printf("%s\n",data);
  }
  uint8_t Stream::read()
  {
	  return serial_getch();
  }
  void Stream::write(uint8_t val)
  {
	serial_putch(val);
  }
  bool Stream::available()
  {
	  
	  return serial_getch_available()?true:false;
  }