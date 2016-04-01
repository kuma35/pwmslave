// PwmSlave.ino
// packet version.

// short packet
// 'EF','65', <<Flag>>, <<addr>>, <<len>>, <<data>>..., <<<sum>>
// Header:EF65 :-)
// Flag:
// ID: 0
// Addr: below
// len: below
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
#include <string.h>

#define DEBUG

#define SERVO_VOLUME 2

// send packet flags
#define FLAG_ACK 0x01	// ignore <<data>> and return only ACK('\x07')
#define FLAG_MEMORY 0x02	// return all memory data to return packet.
#define FLAG_FLASH 0x40	// memory write to EEPROM. (only eeprom-writable data)

// response packet flags
#define ERR_PACKET 0x02	// send packet is syntax or other error.
#define WRN_UNDER_PULSE 0x01
#define WRN_OVER_PULSE  0x04

// in setup(), EEPROM_BASE to EEPROM_LAST copy to Memory same addr.
#define EEPROM_BASE 0
#define EEPROM_I2C_ADDR	0	// NO USE
#define EEPROM_SERVO_0 1 
#define EEPROM_SERVO_1 2
#define EEPROM_SERVO_0_MIN 3	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_0_MAX 5	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MIN 7	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_SERVO_1_MAX 9	// big endian 2 byte int. 0x1234 to 0x12,0x34
#define EEPROM_LAST 11
#define RAM_ATTACH_0 11	// servo_id 0's servo attach command from PC 
#define RAM_ATTACH_1 12	// servo_id 1's servo attach command from PC
#define RAM_PULSE_0  13	// servo_id 0's servo writeMicrosecond command.
#define RAM_PULSE_1  15	// servo_id 1's servo writeMicrosecond command.
#define MEMORY_SIZE  17

#define HEADER_SIZE 2
#define FLAG_SIZE 1
#define ADDR_SIZE 1
#define LEN_SIZE 1
#define SUM_SIZE 1

#define P_SIG1 0xEF
#define P_SIG2 0x65

#define P_HDR_LEN (HEADER_SIZE+FLAG_SIZE+ADDR_SIZE+LEN_SIZE)

#define P_HEADER 0
#define P_FLAG (P_HEADER+HEADER_SIZE)
#define P_ADDR (P_FLAG+FLAG_SIZE)
#define P_LEN (P_ADDR+ADDR_SIZE)
#define P_DATA (P_LEN+LEN_SIZE)

#define R_SIG1 0xEF
#define R_SIG2 0x66

#define R_HEADER 0
#define R_FLAG 2
#define R_ADDR (R_FLAG+1)
#define R_LEN (R_ADDR+1)
#define R_DATA (R_LEN+1)

#define RECV_SIZE 32
#define RESP_SIZE 32

#ifdef DEBUG
SoftwareSerial MySerial(2, 3);
#endif
Servo Sv[SERVO_VOLUME];

unsigned char Memory[MEMORY_SIZE];
unsigned char Recv[RECV_SIZE];	// send packet receive buffer
unsigned char Resp[RESP_SIZE];	// return packet construction buffer

int Recv_index = 0;
int Resp_index = 0;	// next data pointer

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

int order_attach(int id) {
  //func:check register for id 's servo attach order.
  //args:id; servo_id
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

//func: send packet specify start addr and length.
//      copy reciev buffer to specify Memory.
//ret: 0; no-error(error zero)
//     1; addr over MEMORY_SIZE
//     2; addr + len over MEMORY_SIZE. valid data copy is only in Memory addr.
int recv_to_memory(unsigned char *recvp,	// recv[] <<data>> position
		   int len,	// data length
		   unsigned char memory[],
		   int addr) {	// memory addr
  int ret = 0;
  if (addr > MEMORY_SIZE) {
    ret = 1;
    return ret;
  }
  if (addr+len > MEMORY_SIZE) {
    ret = 2;
    len = MEMORY_SIZE - addr;
  }
  for (int i = 0; i < len; i++) {
    #ifdef DEBUG
    MySerial.print(F("memory["));
    MySerial.print(addr+i);
    MySerial.print(F("]="));
    MySerial.print(recvp[i]);
    MySerial.print(F(","));
    #endif
    memory[addr+i] = recvp[i];
  }
  return ret;
}


void drop_packet(void) {
  int c;
#ifdef DEBUG
  MySerial.print(F("DROP PACKET:"));
#endif
  while (Serial.available() > 0) {
    delay(10);
    c = Serial.read();
#ifdef DEBUG
    MySerial.print(c, HEX);
    MySerial.print(F(","));
#endif
  }
#ifdef DEBUG
  MySerial.println(F(""));
#endif
}


// func: spcified bytes data from Serial
// ret: n; bytes == n then complete. n < bytes not complete
int data_recv(int bytes, unsigned char *datap) {
  int index = 0;
  for (; index < bytes ; index++ ) {
    if (Serial.peek() != -1) {
      datap[index] = Serial.read();
    } else {
#ifdef DEBUG
      MySerial.print(F("data_recv break:"));
      MySerial.println(index);
      for (int i=0;i<index;i++) {
	MySerial.print(datap[i], HEX);
	MySerial.print(F(","));
      }
      MySerial.println(F(""));
#endif
      return index;
    }
  }
  MySerial.print(F("data_recv:"));
  MySerial.println(index);
  return index;
}

#ifdef DEBUG
void memory_dump(void) {
  MySerial.print(F("memory dump(HEX):"));
  for (int i = 0; i< MEMORY_SIZE; i++) {
    MySerial.print(Memory[i], HEX);
    MySerial.print(F(","));
  }
  MySerial.println(F(""));
}

void recv_dump(void) {
  MySerial.print(F("recv_packet(HEX):"));
  for (int i = 0; i< RECV_SIZE; i++) {
    MySerial.print(Recv[i], HEX);
    MySerial.print(F(","));
  }
  MySerial.println(F(""));
}

void resp_dump(void) {
  MySerial.print(F("resp_packet(HEX):"));
  for (int i = 0; i< RESP_SIZE; i++) {
    MySerial.print(Resp[i], HEX);
    MySerial.print(F(","));
  }
  MySerial.println(F(""));
}
#endif

void clear_recv(void) {
  memset(Recv, 0, sizeof(Recv));
}


void setup()
{
  Serial.begin(9600);
#ifdef DEBUG
  MySerial.begin(9600);
  //MySerial.println("NOW DEBUGGING20160328");
#endif
  // 
  memset(Memory, 0, sizeof(Memory));	// clear Memory
  // ROM to RAM
  for(int i = EEPROM_BASE; i < EEPROM_LAST; i++) {
    Memory[i] = EEPROM.read(i);
  }
  #ifdef DEBUG
  MySerial.print(F("==========\r\ninitial memory:"));
  memory_dump();
  #endif
}

void loop()
{
  if (Serial.available() > 0) {
    delay(10);
  // set Resp header( minimum resp packet)
    Resp[R_HEADER] = R_SIG1;
    Resp[R_HEADER+1] = R_SIG2;
    Resp[R_FLAG] = 0x00;	// FLag...0;all green.
    Resp[R_ADDR] = 0;
    Resp[R_LEN] = 0;
    #ifdef DEBUG
    MySerial.print(F("========== Serial.available:"));
    MySerial.println(Serial.available());
    #endif
    clear_recv();
    Recv_index = 0;
    if (data_recv(P_HDR_LEN, Recv+Recv_index) == P_HDR_LEN) {
      Recv_index += P_HDR_LEN;
    } else {
      drop_packet();
      Resp[R_FLAG] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_DATA-P_FLAG);
#ifdef DEBUG
      resp_dump();
#endif
      Serial.write(Resp, R_LEN+2);
#ifdef DEBUG
      MySerial.println("Err Recv HDRSIZE");
#endif
      return;
    }
#ifdef DEBUG
    recv_dump();
#endif
    if (Recv[P_HEADER] != P_SIG1 || Recv[P_HEADER+1] != P_SIG2) {
      drop_packet();
      Resp[2] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_DATA-R_FLAG);
#ifdef DEBUG
      resp_dump();
#endif
      Serial.write(Resp, R_LEN+2);
#ifdef DEBUG
      MySerial.println("Err Recv HDR");
#endif
      return;
    }
    if (data_recv(Recv[P_LEN], Recv+Recv_index) == Recv[P_LEN]) {
      Recv_index = P_DATA + Recv[P_LEN];
    } else {
      drop_packet();
      Resp[R_FLAG] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_DATA-R_FLAG);
#ifdef DEBUG
      resp_dump();
#endif
      Serial.write(Resp, R_LEN+2);
#ifdef DEBUG
      MySerial.println("Err Recv DATA");
#endif
      return;
    }
    switch (recv_to_memory(Recv+P_DATA,
			   Recv[P_LEN],
			   Memory,
			   Recv[P_ADDR])) {
    case 0: // no error
      break;
    case 1:
    case 2:
      drop_packet();
      Resp[R_FLAG] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_DATA-R_FLAG);
      Serial.write(Resp, R_LEN+2);
      return;
    }
    if (data_recv(1, Recv+Recv_index) == 1) {
      Recv_index += 1;
    } else {
      drop_packet();
      Resp[R_FLAG] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_DATA-R_FLAG);
#ifdef DEBUG
      resp_dump();
#endif
      Serial.write(Resp, R_LEN+2);
#ifdef DEBUG
      MySerial.println("Err Recv SUM");
#endif
      return;
    }
#ifdef DEBUG
    recv_dump();
    MySerial.print(F("Recv_index:"));
    MySerial.println(Recv_index);
    MySerial.print(F("Recv checksum:"));
    MySerial.println(Recv[Recv_index-1]);
    MySerial.print(F("get_checksum:"));
    MySerial.println(get_checksum(Recv+P_FLAG, Recv_index - 1 - P_FLAG));
    memory_dump();
#endif
    if (Recv[Recv_index-1] != get_checksum(Recv+P_FLAG, Recv_index - 1 - P_FLAG)) {
      drop_packet();
      Resp[R_FLAG] |= ERR_PACKET;
      Resp[R_LEN+1] = get_checksum(Resp+R_FLAG, R_FLAG-R_LEN);
#ifdef DEBUG
      resp_dump();
#endif
      Serial.write(Resp, R_LEN+2);
#ifdef DEBUG
      MySerial.println("Err checksum");
#endif
      return;
    }
    // checking flags
    if (Recv[P_FLAG] & FLAG_ACK) {
      Serial.write(0x07);	// ACK return only \0x07
#ifdef DEBUG
      MySerial.println("ret ACK");
#endif
      return; // RETURN!! //
    }
    if (Recv[P_FLAG] & FLAG_FLASH) {
      MySerial.println(F("processing FLASH..."));
      for (int adr =0; adr < EEPROM_LAST; adr++) {
	if (EEPROM.read(adr) != Memory[adr]) {
	  EEPROM.write(adr, Memory[adr]);
	}
      }
      Resp[R_ADDR] = 0;
      Resp[R_LEN] = 0;
      Resp[R_DATA] = get_checksum(Resp+P_FLAG, R_DATA - R_FLAG);
      Resp_index = R_DATA + 1;
      //#ifdef DEBUG
      //      resp_dump();
      //#endif
      //Serial.write(Resp, Resp_index);
    }
    if (Recv[P_FLAG] & FLAG_MEMORY) {
      MySerial.println(F("processing MEMORY..."));
      Resp[R_ADDR] = 0;
      Resp[R_LEN] = MEMORY_SIZE;
      for (int addr =0; addr < MEMORY_SIZE; addr++) {
	Resp[R_DATA+addr] = Memory[addr];
      }
      Resp_index = R_DATA + MEMORY_SIZE;
      //Resp[Resp_index] = get_checksum(Resp+R_FLAG, Resp_index-R_FLAG);
      //#ifdef DEBUG
      //      resp_dump();
      //#endif
      //Serial.write(Resp, Resp_index);
    }
    
    // memory check and run
    for (int servo_id = 0; servo_id < 2; servo_id++) {
      // attach
      int flag = order_attach(servo_id);
      if (flag != 0) {
	if (!Sv[servo_id].attached()) {
	  Sv[servo_id].attach(Memory[EEPROM_SERVO_0+servo_id]);
	}
      } else {
	if (Sv[servo_id].attached()) {
	  Sv[servo_id].detach();
	}
      }

      // pulse
      if (Sv[servo_id].attached()) {
	int pulse = get_pulse(servo_id);
	int min = get_servo_min(servo_id);
	int max = get_servo_max(servo_id);
	if (pulse < min) {
	  Resp[R_FLAG] |= WRN_UNDER_PULSE;
	  pulse = min;
	} else if ( pulse > max) {
	  pulse = max;
	  Resp[R_FLAG] |= WRN_OVER_PULSE;
	}
	Sv[servo_id].writeMicroseconds(pulse);
      }
    }
    // post execute status to resp.
    // only set R_FLAG and re checksum then Serial.write()
    MySerial.println(F("return executed return packet."));
    //Resp[R_FLAG] |= 0; // error zero.
    Resp[Resp_index] = get_checksum(Resp+R_FLAG, Resp_index-R_FLAG);
#ifdef DEBUG
    resp_dump();
#endif
    Serial.write(Resp, Resp_index+1);
  }
}
