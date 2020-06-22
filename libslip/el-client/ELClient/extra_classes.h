#ifndef _EXTRA_DEFINES_H
#define _EXTRA_DEFINES_H

#include <stdint.h>
class Stream
{
  int stub;  
public:
  //void Request( unsigned char* data, unsigned int len); 
  void print(const char * data);
  void print(uint16_t data);
  void print(uint32_t, int);
  void println();
  void println(uint32_t);
  void println(const char * data);
  
  
  
  uint8_t read();
  void write(uint8_t);
  bool available();
};

#define NULL  0
#endif
