// +---------------------------------------------------------------------------+
// | Mini serial console * v0.1                                                |
// | Copyright (C) 2022 Pozsár Zsolt <pozsarzs@gmail.com>                      |
// | mini_serial_console.cpp                                                   |
// | Program for Raspberry Pi Pico                                             |
// +---------------------------------------------------------------------------+

//   This program is free software: you can redistribute it and/or modify it
// under the terms of the European Union Public License 1.2 version.
//
//   This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.

// settings
const int     spd_serialUSB     = 115200;  // speed of the USB serial port
const int     spd_serial0       = 38400;   // speed of the TTL serial port
const int     spd_serial1       = 38400;   // speed of the RS232C serial port
// GPIO ports
const int     lcd_bl            = 14;      // LCD - backlight on/off
const int     lcd_db0           = 2;       // LCD - databit 0
const int     lcd_db1           = 3;       // LCD - databit 1
const int     lcd_db2           = 4;       // LCD - databit 2
const int     lcd_db3           = 5;       // LCD - databit 3
const int     lcd_db4           = 10;      // LCD - databit 4
const int     lcd_db5           = 11;      // LCD - databit 5
const int     lcd_db6           = 12;      // LCD - databit 6
const int     lcd_db7           = 13;      // LCD - databit 7
const int     lcd_en            = 7;       // LCD - enable
const int     lcd_rs            = 6;       // LCD - register select
const int     prt_jp2           = 16;      // operation mode (JP2 jumper)
const int     prt_jp3           = 15;      // operation mode (JP3 jumper)
const int     prt_pb0           = 17;      // pushbutton 0
const int     prt_pb1           = 18;      // pushbutton 1
const int     prt_pb2           = 19;      // pushbutton 2
const int     prt_pb3           = 20;      // pushbutton 3
const int     prt_pb4           = 21;      // pushbutton 4
const int     prt_pb5           = 22;      // pushbutton 5
const int     prt_rxd0          = 1;       // RXD line of the serial port 0
const int     prt_txd0          = 0;       // TXD line of the serial port 0
const int     prt_rxd1          = 9;       // RXD line of the serial port 1
const int     prt_txd1          = 8;       // TXD line of the serial port 1
// general constants
const String  swversion         = "0.1";   // version of this program
// general variables
int           opmode            = 0;       // operation mode
// messages
String msg[40]                  =
{
  /*  0 */  "",
  /*  1 */  "Mini serial console",
  /*  2 */  "with two serial port",
  /*  3 */  "Software: v",
  /*  4 */  "(C)2022 Pozsar Zsolt",
  /*  5 */  "Initializing...",
  /*  6 */  "",
  /*  7 */  "",
  /*  8 */  "",
  /*  9 */  "",
  /* 10 */  "",
  /* 11 */  "",
  /* 12 */  "",
  /* 13 */  "",
  /* 14 */  "",
  /* 15 */  "",
  /* 16 */  "",
  /* 17 */  "",
  /* 18 */  "",
  /* 19 */  "",
  /* 20 */  "",
  /* 21 */  "",
  /* 22 */  "",
  /* 23 */  "",
  /* 24 */  "",
  /* 25 */  "",
  /* 26 */  "",
  /* 27 */  "",
  /* 28 */  "",
  /* 29 */  "",
  /* 30 */  "",
  /* 31 */  "",
  /* 32 */  "",
  /* 33 */  "",
  /* 34 */  "",
  /* 35 */  "",
  /* 36 */  "",
  /* 37 */  "",
  /* 38 */  "",
  /* 39 */  "",
};


UART Serial1(prt_txd0,prt_rxd0,0,0);
UART Serial2(prt_txd1,prt_rxd1,0,0);

void setup() {
  // set USB serial port
  Serial.begin(spd_serialUSB);
  // write program information
  Serial.println("");
  Serial.println("");
  Serial.println(msg[1] + ' ' + msg[2]);
  Serial.println(msg[3] + swversion );
  Serial.println(msg[4]);
  Serial.println(msg[5]);
  // set other serial ports
  Serial1.begin(spd_serial0);
  Serial2.begin(spd_serial1);


}

void loop() {


}

