// TinyShell class body
// 2015/10/24 by kuma35

#include <ctype.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include "TinyShell.h"

#define ERR_MSG_UNDERFLOW " Stack underflow."
#define ERR_MSG_OVERFLOW " Stack overflow."
#define ERR_MSG_UNKNOWN " unknow word."
#define CR '\x0d'
#define CRLF "\r\n"

const int TinyShell::NEWLINE = 0x0A;

int TinyShell::do_help(void) {
  this->_serial->print(F(" ( ... -- ... ) stack effect. ( pre command stack -- post command stack" CRLF \
			 "help  ( -- ) is this command." CRLF		\
			 ".  ( n -- ) print number." CRLF));
  return 1;
}


int TinyShell::do_number_print(void) {
  // .  ( n -- ) print number.
  // ret:1;done, 0;error
  if (!this->_data_stack.popable(1)) {
    this->_serial->println(ERR_MSG_UNDERFLOW);
    return 0;
  }
  this->_serial->print(this->_data_stack.pop());
  return 1;
}

TinyShell::TinyShell()
  : _serial(&Serial) {
  //
  //this->_data_stack = new Stack();
  //this->_line_buffer = new String();
}

// ret:1 is number and number storing to num.
// ret:0 is not number. num no edited.
int TinyShell::isDigits(String *strp, int *nump) {
  int index = 0;
  if ((*strp)[index] != '-' &&
      (*strp)[index] != '+' &&
      !isdigit((*strp)[index])) {
    return 0;
  }
  for (index = 1; index < strp->length(); index++) {
    if (!isdigit((*strp)[index])) {
	return 0;
      }
  }
  *nump = atoi(strp->c_str());
  return 1;
}


// func: get line ( to newline ) and return last char. 
int TinyShell::get_line(void) {
  //String &line_buffer
  char c[2] = {'\0', '\0'};
  //Serial.print("get_line:");
  while (this->_serial->available()) {
    c[0] = (char)this->_serial->read();
    //this->_serial->print(F(","));
    //this->_serial->print(c[0], HEX);
    this->_line_buffer += c;
    if (c[0] == CR) {
      this->_serial->println();	// echo newline
      break;
    } else {
      this->_serial->print(c);	// echo
    }
  }
  //this->_serial->println("");
  return c[0];
}


// ret:0 token end
// ret:1 token got
int TinyShell::get_token(String *tokenp, String sep) {
  int index;
  this->_line_buffer.trim();
  if (!this->_line_buffer.equals("")) {
    index = this->_line_buffer.indexOf(sep, 0);
    if (index > -1) {
      *tokenp = this->_line_buffer.substring(0, index);
      this->_line_buffer = this->_line_buffer.substring(index+1);
    } else {
      *tokenp = this->_line_buffer;
      this->_line_buffer = "";
    }
    return 1;
  } else {
    return 0;
  }
}

// ret:0 not number
// ret:1 number
int TinyShell::get_number(String *tokenp) {
  int num;
  if (this->isDigits(tokenp, &num)) {
    if (this->_data_stack.pushable(1)) {
      this->_data_stack.push(num);
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

String TinyShell::buffer(void) {
  return this->_line_buffer;
}

void TinyShell::clear_buffer(void) {
  this->_line_buffer = "";
}

int TinyShell::available(void) {
  return this->_line_buffer.equals("");
}

void TinyShell::set_line(String line) {
  this->_line_buffer += line;
}

int TinyShell::is_newline(void) {
  static const char sep[2] = {this->NEWLINE, 0};
  return this->_line_buffer.indexOf(sep,0) > -1;
}

// 適宜オーバーライドされたし。
// ret:>=1; command result code is plus 100. 1to100;NG, 101to;OK
// ret:0; unknown word
int TinyShell::execute(String *tokenp) {
  //this->_serial->print(F("execute:"));
  //this->_serial->println(*tokenp);
  int status = 100;	// execute return status. command result code is plus 100.
  if (tokenp->equals("help")) {	// ( -- ) print help message.
    status += this->do_help();
  } else if (tokenp->equals(".")) {	// ( n -- ) print number.
    status += this->do_number_print();
  } else if (tokenp->equals("cr")) {	// ( -- ) print newline.
    this->_serial->println();
    status += 1;
  } else if (tokenp->equals("dup")) {	// ( n -- n n )
    status += this->_data_stack.dup();
    if (status == 100) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_OVERFLOW);
    } else if (status == 99) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("swap")) {	// ( x y -- y x)
    status += this->_data_stack.swap();
    if (status == 100) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("drop")) {	// ( n -- )
    status += this->_data_stack.drop();
    if (status == 100) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("over")) {	// ( x y -- x y x)
    status += this->_data_stack.over();
    if (status == 100) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_OVERFLOW);
    } else if (status == 99) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("rot")) {	// ( n1 n2 n3 -- n2 n3 n1)
    status += this->_data_stack.rot();
    if (status == 100) {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("eeprom-write")) {	// ( n addr -- ) writing n to eeprom-addr.
    if (this->_data_stack.popable(2)) {
      int addr = this->_data_stack.pop();
      int value = this->_data_stack.pop();
      EEPROM.write(addr, (unsigned char)value);
      // EEPROMへの書き込みには3.3ミリ秒かかります。
      // EEPROMの書込/消去サイクルは100,000回で寿命に達します。頻繁に書き込みを行う場合は注意してください
      status += 1;	// 1;OK, status 101
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("eeprom-2write")) {	// ( 2n addr -- ) writing 2n to eeprom-addr
    // big endian 0x1234 to 0x12, 0x34
    if (this->_data_stack.popable(2)) {
      int addr = this->_data_stack.pop();
      int value = this->_data_stack.pop();
      EEPROM.write(addr, (unsigned char)((value >> 8) & 0xff));
      EEPROM.write(addr+1, (unsigned char)(value & 0xff));
      // EEPROMへの書き込みには3.3ミリ秒かかります。
      // EEPROMの書込/消去サイクルは100,000回で寿命に達します。頻繁に書き込みを行う場合は注意してください
      status += 1;	// 1;OK, status 101
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("eeprom-read")) {	// ( addr -- n ) reading n from eeprom-addr.
    if (this->_data_stack.popable(1)) {
      int addr = this->_data_stack.pop();
      int value = EEPROM.read(addr);
      if (this->_data_stack.pushable(1)) {
	this->_data_stack.push(value);
	status += 1;
      } else {
	this->_serial->print(tokenp->c_str());
	this->_serial->println(ERR_MSG_OVERFLOW);
	status -= 1;
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("eeprom-2read")) {	// ( addr -- 2n ) reading 2n from eeprom-addr.
    if (this->_data_stack.popable(1)) {
      int addr = this->_data_stack.pop();
      int value = EEPROM.read(addr) << 8;
      value |= EEPROM.read(addr+1);
      if (this->_data_stack.pushable(1)) {
	this->_data_stack.push(value);
	status += 1;
      } else {
	this->_serial->print(tokenp->c_str());
	this->_serial->println(ERR_MSG_OVERFLOW);
	status -= 1;
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(ERR_MSG_UNDERFLOW);
    }
  } else if (tokenp->equals("dump")) {	// ( -- ) print stack dump.
    this->_data_stack.dump(this->_serial);
    status += 1;
  } else {
    // unknown word
    this->_serial->print(tokenp->c_str());
    this->_serial->println(ERR_MSG_UNKNOWN);
    status = 0;
  }
  return status;
}
