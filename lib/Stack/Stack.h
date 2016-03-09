// Stack class header
// 2013/07/04 by kuma35
// for push:
// if (this->pushable()) push(), etc.
// for pop
// if (this->popable()) pop(), etc.

#ifndef _Stack_h_
#define _Stack_h_
//#include <arduino.h>
#include <HardwareSerial.h>

#define STACK_SIZE 16

class Stack {
 private:
  //static const int _size_default = STACK_SIZE_DEFAULT_INIT;
  //static const int _empty = -1;
  static const int _size = STACK_SIZE;
  static const int _empty = -1;
  int _stack[STACK_SIZE];
  int _pointer;
 public:
 Stack();
  bool popable(int depth = 1);
  int pop(void);
  bool pushable(int depth = 1);
  void push(int value);
  void clear(void);
  void dump(HardwareSerial *serialp);
  int do_depth(void);
  int dup(void);
  int swap(void);
  int drop(void);
  int over(void);
  int rot(void);
};

#endif
