// PwmSlave.ino
// packet version.

// short packet
// 'EF','65', <<Flag>>, <<ID>>, <<addr>>, <<len>>, <<count>>, <<data>>..., <<<sum>>
// Header:EF65 :-)
// Flag:
// ID: 0
// Addr: below
// len: below
// count: 1
// data: below
// sum:
//
// return packet
// 'EF', '66', <<flag>> 

// virtual register
// 0-10 copy from EEPROM at setup(). FLAG_FLASH is 1 then write to EEPROM.
// 11-  register. attach and pulse.
// addr description                 default
// ==== =========================== =======
//    0 I2C Address                       0
//    1 Servo 0 digital port number       5
//    2 Servo 1 digital port number       6
//    3 Servo 0 minimum pulse (L,H)     800
//    4 -                                 -
//    5 Servo 0 maximam pulse (L,H)    2200
//    6 -                                 -
//    7 Servo 1 minimum pulse (L,H)     800
//    8 -                                 -
//    9 Servo 1 maximam pulse (L,H)    2200
//   10 -                                 -
//   11 Servo 0 attach flag               0
//   12 Servo 1 attach flag               0
//   13 Servo 0 pulse (L,H)            1100
//   14 -                                 -
//   15 Servo 1 pulse (L,H)            1100
//   16 -                                 -


#include <EEPROM.h>
#include <HardwareSerial.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define DEBUG

#define MEMORY_SIZE 32
#define RECV_SIZE 32
#define RESP_SIZE 32
#define SERVO_VOLUME 2

#define FLAG_ACK 0x01
#define FLAG_FLASH 0x40

#define EEPROM_BASE 0
#define EEPROM_I2C_ADDR	0	// NO USE
#define EEPROM_SERVO_0 1 
#define EEPROM_SERVO_1 2
#define EEPROM_SERVO_0_MIN 3	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_0_MAX 5	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MIN 7	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MAX 9	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_LAST 11
#define RAM_ATTACH_0 11
#define RAM_ATTACH_1 12
#define RAM_PULSE_0  13
#define RAM_PULSE_1  15

#ifdef DEBUG
SoftwareSerial MySerial(2, 3);
#endif
Servo Sv[SERVO_VOLUME];

unsigned char Memory[MEMORY_SIZE];
unsigned char Recv[RECV_SIZE];
unsigned char Resp[RESP_SIZE];


int Recv_index = 0;

int get_servo_pin(int id) {
  switch (id) {
  case 0:
    return Memory[EEPROM_SERVO_0];
  case 1:
    return Memory[EEPROM_SERVO_1];
  } 
}

int get_servo_min(int id) {
  switch (id) {
  case 0:
    return Memory[EEPROM_SERVO_0_MIN] << 8 |
      Memory[EEPROM_SERVO_0_MIN+1];
  case 1:
    return Memory[EEPROM_SERVO_1_MIN] << 8 |
      Memory[EEPROM_SERVO_1_MIN+1];
  }
}

int get_servo_max(int id) {
  switch (id) {
  case 0:
    return Memory[EEPROM_SERVO_0_MAX] << 8 |
      Memory[EEPROM_SERVO_0_MAX+1];
  case 1:
    return Memory[EEPROM_SERVO_1_MAX] << 8 |
      Memory[EEPROM_SERVO_1_MAX+1];
  }
}

int get_attach(int id) {
  switch (id) {
  case 0:
    return Memory[RAM_ATTACH_0];
  case 1:
    return Memory[RAM_ATTACH_1];
  }
}

int get_pulse(int id) {
  switch (id) {
  case 0:
    return Memory[RAM_PULSE_0] << 8 |
      Memory[RAM_PULSE_0+1];
  case 1:
    return Memory[RAM_PULSE_1] << 8 |
      Memory[RAM_PULSE_1+1];
  }
}

int get_checksum(unsigned char *datap, int len) {
  int sum = datap[0];
  for (int i = 1; i < len; i++) {
    sum ^= datap[i];
  }
  return sum;
}

int recv_to_memory(unsigned char *recvp, int len, int addr, unsigned char memory[]) {
  for (int i = 0; i < len; i++) {
    memory[addr+i] = recvp[i];
  }
}



void drop_packet(void) {
  while (Serial.available() > 0) {
    delay(10);
    Serial.read();
  }
}

int data_recv(int bytes, unsigned char *datap) {
  for (int index = 0; index < bytes ; index++ ) {
    if (Serial.peek() != -1) {
      datap[index] = Serial.read();
    } else {
      return 0;
    }
  }
  return 1;
}

#ifdef DEBUG
void memory_dump(void) {
  for (int i = 0; i< MEMORY_SIZE; i++) {
    MySerial.print(Memory[i], HEX);
    //print_hex(Memory[i]);
    MySerial.print(F(","));
  }
  MySerial.println(F(""));
}
#endif

void setup()
{
#ifdef DEBUG
  MySerial.begin(9600);
  MySerial.println("NOW DEBUGGING");
#endif
  Serial.begin(9600);
  // ROM to RAM
  for(int i = EEPROM_BASE; i < EEPROM_LAST; i++) {
    Memory[i] = EEPROM.read(i);
  }
#ifdef DEBUG
  memory_dump();
#endif
}

void loop()
{
  int data = -1;
  if (Serial.available() > 0) {
    delay(10);
    Recv_index = 0;
    if (!data_recv(7, Recv+Recv_index)) {
      drop_packet();
      return;
    }
#ifdef DEBUG
    MySerial.print(F("read header:"));
    memory_dump();
#endif
    if (Recv[0] != 0xEF || Recv[1] != 0x65) {
      drop_packet();
      return;
    }
    if (Recv[2] & FLAG_ACK) {
      drop_packet();
      Serial.write(0x07);
      return;
    }
    if (Recv[2] & FLAG_FLASH) {
      drop_packet();
      for (int adr; adr < EEPROM_LAST; adr++) {
	if (EEPROM.read(adr) != Memory[adr]) {
	  EEPROM.write(adr, Memory[adr]);
	}
      }
    }
    Recv_index += 7;
    if (!data_recv(Recv[5], Recv+Recv_index)) {
      drop_packet();
      return;
    }
    recv_to_memory(Recv+7, Recv[4], Recv[5], Memory);
    Recv_index += Recv[5];
    if (!data_recv(1, Recv+Recv_index)) {
      drop_packet();
      return;
    }
#ifdef DEBUG
    MySerial.print(F("Recv_index:"));
    MySerial.println(Recv_index);
    MySerial.print(F("recv packet:"));
    memory_dump();
    MySerial.print(F("get_checksum"));
    MySerial.println(get_checksum(Recv+3, Recv_index - 1));
#endif
    if (Recv[Recv_index] != get_checksum(Recv+3, Recv_index - 1)) {
      drop_packet();
      return;
    }
  }
  // memory check and run
  for (int servo_id = 0; servo_id < 2; servo_id++) {
    int pin = get_attach(servo_id);
    if (pin != 0) {
      if (!Sv[servo_id].attached()) {
	Sv[servo_id].attach(pin);
      }
      // writeMicrosecond only attached.
      int pulse = get_pulse(servo_id);
      int min = get_servo_min(servo_id);
      int max = get_servo_max(servo_id);
      if (pulse < min) {
	  pulse = min;
      } else if ( pulse > max) {
	pulse = max;
      }
      Sv[servo_id].writeMicroseconds(pulse);
    } else {
      if (Sv[servo_id].attached()) {
	Sv[servo_id].detach();
      }
    }
  }
}
