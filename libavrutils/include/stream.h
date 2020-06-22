#ifndef __STREAM_H
#define __STREAM_H

#include <stdint.h>

class Stream
{

  typedef uint8_t 	(*_p_read)();
  typedef void 		(*_p_write)(uint8_t val);
  typedef uint8_t	(*_p_available)();
  
  _p_read  _read;
  _p_write _write;
  _p_available _available;
  

 
public:
	Stream(_p_read,_p_write,_p_available);
   
   
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

#endif