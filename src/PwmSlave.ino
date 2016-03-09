// PwmSlave.ino

#include <EEPROM.h>
#include <HardwareSerial.h>
#include <Servo.h>
#include <string.h>

#include "TinyShell.h"

#define SERVO_VOLUME 2
#define CR '\x0d'
#define LF '\x0a'
#define ERR_STACK_OVERFLOW "Stack over flow"
#define EEPROM_I2C_ADDR	0
#define EEPROM_SERVO_0 1 
#define EEPROM_SERVO_1 2
#define RECEIVE_BUF_LEN 130
#define EEPROM_SERVO_0_MIN 3	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_0_MAX 5	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MIN 7	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MAX 9	// big endian 2 byte int. 0x1234 to 0x12,0x34

Servo Servos[SERVO_VOLUME];
int Servo_min[SERVO_VOLUME];
int Servo_max[SERVO_VOLUME];
int Servo_pin[SERVO_VOLUME];

class PwmSlaveShell : public TinyShell {
 public:
  int execute(String *tokenp);
};

int PwmSlaveShell::execute(String *tokenp) {
  int status = 100;
  if (tokenp->equals("pulse")) {	// ( <<servo_no>>,<<microseonds>> --)	local rc servo writeMicroseconds
    if (this->_data_stack.popable(2)) {
      int value = this->_data_stack.pop();
      int servo_no = this->_data_stack.pop();
      if ( 0 <= servo_no and servo_no < SERVO_VOLUME) {
	Servos[servo_no].writeMicroseconds(value);
	status += 1;
      } else {
	this->_serial->println(F("Out of servo no."));
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(" Stack underflow.");
    }
  } else if (tokenp->equals("angle")) {	// ( <<servo_no>>,<<degbree>> --)	local rc servo write. 0 to 180.
    if (this->_data_stack.popable(2)) {
      int value = this->_data_stack.pop();
      int servo_no = this->_data_stack.pop();
      if ( 0 <= servo_no and servo_no < SERVO_VOLUME) {
	Servos[servo_no].write(value);
	status += 1;
      } else {
	this->_serial->println(F("Out of servo no."));
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(" Stack underflow.");
    }
  } else if (tokenp->equals("attach")) {	// <<servo_no>> -- attach to Digital pin to servo.
    if (this->_data_stack.popable(1)) {
      int servo_no = this->_data_stack.pop();
      if ( 0 <= servo_no and servo_no < SERVO_VOLUME) {
	Servos[servo_no].attach(Servo_pin[servo_no], Servo_min[servo_no], Servo_max[servo_no]);
	status += 1;
      } else {
	this->_serial->println(F("Out of servo no."));
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(" Stack underflow.");
    }
  } else if (tokenp->equals("detach")) {	// <<servo_no>> -- detach from Digital pin to servo.
    if (this->_data_stack.popable(1)) {
      int servo_no = this->_data_stack.pop();
      if ( 0 <= servo_no and servo_no < SERVO_VOLUME) {
	Servos[servo_no].detach();
	status += 1;
      } else {
	this->_serial->println(F("Out of servo no."));
      }
    } else {
      this->_serial->print(tokenp->c_str());
      this->_serial->println(" Stack underflow.");
    }
  } else {
    status = TinyShell::execute(tokenp);
  }
  return status;
}

PwmSlaveShell Shell;

void setup()
{
  Serial.begin(9600);
  Servo_pin[0] = EEPROM.read(EEPROM_SERVO_0);
  Serial.print(F("Servo 0 to D"));
  Serial.print(Servo_pin[0]);
  //Servos[0].attach(servo_pin);
  Servo_min[0] = ((int)EEPROM.read(EEPROM_SERVO_0_MIN) << 8) |
    (EEPROM.read(EEPROM_SERVO_0_MIN+1));
  Servo_max[0] = ((int)EEPROM.read(EEPROM_SERVO_0_MAX) << 8) |
    (EEPROM.read(EEPROM_SERVO_0_MAX+1));
  Serial.print(F(" "));
  Serial.print(Servo_min[0]);
  Serial.print(F(" - "));
  Serial.println(Servo_max[0]);
  
  Servo_pin[1] = EEPROM.read(EEPROM_SERVO_1);
  Serial.print(F("Servo 1 to D"));
  Serial.print(Servo_pin[1]);
  //Servos[1].attach(servo_pin);
  Servo_min[1] = ((int)EEPROM.read(EEPROM_SERVO_1_MIN) << 8) |
    (EEPROM.read(EEPROM_SERVO_1_MIN+1));
  Servo_max[1] = ((int)EEPROM.read(EEPROM_SERVO_1_MAX) << 8) |
    (EEPROM.read(EEPROM_SERVO_1_MAX+1));
  Serial.print(F(" "));
  Serial.print(Servo_min[1]);
  Serial.print(F(" - "));
  Serial.println(Servo_max[1]);

  Serial.print(">");
}

String buffer;
String token;

void loop()
{
  if (Serial.available() >0) {
    delay(10);
    int last_char = Shell.get_line();
    if (last_char == CR) {
      while (Shell.get_token(&token)) {
	if (Shell.get_number(&token)) {
	} else if (Shell.execute(&token) <= 100) {
	  // command execution status:
	  // 0;unkown word
	  // 100<=status;command execute done. and error
	  // 101>=status;command execute done. and success continue next word execute...
	  Serial.print(F("stop. not yet exec words: "));
	  Serial.println(Shell.buffer().c_str());
	  Shell.clear_buffer();
	  break;	// stop interpreting
	}
      }
      Serial.print(F(">"));
    } else {
      // nothing
    }
  }
 }
