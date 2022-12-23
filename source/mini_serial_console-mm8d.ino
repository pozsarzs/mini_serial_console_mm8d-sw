// +---------------------------------------------------------------------------+
// | Mini serial console for MM8D * v0.1                                       |
// | Copyright (C) 2022 Pozsár Zsolt <pozsarzs@gmail.com>                      |
// | mini_serial_console-mm8d.ino                                              |
// | Program for Raspberry Pi Pico                                             |
// +---------------------------------------------------------------------------+

/*
    This program is free software: you can redistribute it and/or modify it
  under the terms of the European Union Public License 1.2 version.

    This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.
*/

/*
  Operation modes
  ~~~~~~~~~~~~~~~

    Mode #0:   read only mode,
               no cursor,
               20x4 size displayed area on 80x4 size virtual screen,
               automatically scrolling lines,
               the displayed area can be moved horizontally with pushbuttons.
    Mode #1:   read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               automatically scrolling lines,
               the displayed area can be moved horizontally and vertically with pushbuttons.
    Mode #2:   read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               the displayed area can be moved horizontally and vertically with pushbuttons,
               after FormFeed (0x12), a new, clean page starts.
    Mode #3/0: read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               the displayed page can be change with pushbuttons,
               switch to other submode with PB4 button.
    Mode #3/1: read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               the displayed page can be change with pushbuttons,
               switch to other submode with PB4 button.
    Mode #3/2: read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               automatically scrolling lines,
               the displayed area can be moved horizontally and vertically with
               pushbuttons,
               switch to other submode with PB4 button.
               lock scroll with PB5 button

    Note:
     '*': You can set size of the virtual screen with virtscreenxsize and
          virtscreenysize constants in operation mode #1 and #2.


  Button functions
  ~~~~~~~~~~~~~~~~

    Button  Mode #0     Mode #1     Mode #2     Mode #3/0   Mode #3/1   Mode #3/2
    -------------------------------------------------------------------------------
     PB0    move left   move left   move left                           move left
     PB1    move right  move right  move right                          move right
     PB2                move up     move up     page up     page up     scroll up
     PB3                move down   move down   page down   pahe down   scroll down
     PB4                                        submode     submode     submode
     PB5                                                                lock scroll


  Serial ports
  ~~~~~~~~~~~~

    Serial0:  via USB port
    Serial1:  UART0  TTL    0/3.3V
    Serial2:  UART1  RS232  +/-12V


  Example incoming (binary) data lines in Mode #3
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

     0123456789ABC
    "CH..........."

    0:   'C'                                  0x43
    1:   'H'                                  0x48
    2:   number of channel                    0x00
    3:   overcurrent breaker error            0x00: closed 0x01: opened
    4:   water pump pressure error (no water) 0x00: good   0x01: bad
    5:   water pump pressure error (clogging) 0x00: good   0x01: bad
    6:   external temperature in °C          (0x00-0x80)
    7:   status of water pump and tube #1     0x00: off    0x01: on     0x02: always off 0x03: always on
    8:   status of water pump and tube #2     0x00: off    0x01: on     0x02: always off 0x03: always on
    9:   status of water pump and tube #3     0x00: off    0x01: on     0x02: always off 0x03: always on
    A-C: unused                               0x00

    0:   'C'                                  0x43
    1:   'H'                                  0x48
    2:   number of channel                    0x01-0x08
    3:   temperature in °C                   (0x00-0x80)
    4:   relative humidity                   (0x00-0x80)
    5:   relative unwanted gas concentrate   (0x00-0x80)
    6:   operation mode                       0x00: hyphae 0x01: mushr. 0x7F: disabled channel
    7:   manual mode                          0x00: auto   0x01: manual
    8:   overcurrent breaker error            0x00: closed 0x01: opened
    9:   status of door (alarm)               0x00: closed 0x01: opened
    A:   status of lamp output                0x00: off    0x01: on     0x02: always off 0x03: always on
    B:   status of ventilator output          0x00: off    0x01: on     0x02: always off 0x03: always on
    C:   status of heater output              0x00: off    0x01: on     0x02: always off 0x03: always on


  Example incoming (text) log lines in Mode #3
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

       0      1    2           3
    "221213 114421 I Configuration is loaded."
    "221213 114427 W CH2: MM6D is not accessible."
    "221213 114427 E ERROR #18: There is not enabled channel!"

    0:   date in yymmdd format
    1:   time in hhmmss format
    2:   level (information, warning, error)
    3:   short description
*/

#define LCD_8BIT                                            // enable 8 bit mode of the LCD
// #define COM_USB                                          // enable Serial #0 port
#define COM_TTL                                             // enable Serial #1 port
#define COM_RS232C                                          // enable Serial #2 port
// #define COM_USB_MESSAGES                                 // enable console messages on Serial #0 port
// #define COM_TTL_MESSAGES                                 // enable console messages on Serial #1 port
// #define COM_RS232C_MESSAGES                              // enable console messages on Serial #2 port
// #define TEST                                             // enable test mode of scrolling

#define BEL   0x07                                          // ASCII code of the control characters
#define TAB   0x09
#define FF    0x0B
#define SPACE 0x20
#define DEL   0x7F

#include <LiquidCrystal.h>

// settings
const int     lcd_bloffinterval       = 60000;              // LCD backlight off time after last button press
const byte    lcd_xsize               = 20;                 // horizontal size of display
const byte    lcd_ysize               = 4;                  // vertical size of display
const byte    virtscreenxsize         = 80;                 // horizontal size of virtual screen
const byte    virtscreenysize         = 25;                 // vertical size of virtual screen
// serial ports
const int     com_speed[3]            = {115200,
                                         9600,
                                         9600
                                        };                  // speed of the USB serial port
#ifdef ARDUINO_ARCH_MBED_RP2040
const byte    com_rxd2                = 8;
const byte    com_txd2                = 9;
#endif
// GPIO ports
const byte    lcd_bl                  = 14;                 // LCD - backlight on/off
const byte    lcd_db0                 = 2;                  // LCD - databit 0
const byte    lcd_db1                 = 3;                  // LCD - databit 1
const byte    lcd_db2                 = 4;                  // LCD - databit 2
const byte    lcd_db3                 = 5;                  // LCD - databit 3
const byte    lcd_db4                 = 10;                 // LCD - databit 4
const byte    lcd_db5                 = 11;                 // LCD - databit 5
const byte    lcd_db6                 = 12;                 // LCD - databit 6
const byte    lcd_db7                 = 13;                 // LCD - databit 7
const byte    lcd_en                  = 7;                  // LCD - enable
const byte    lcd_rs                  = 6;                  // LCD - register select
const byte    prt_jp2                 = 16;                 // operation mode (JP2 jumper)
const byte    prt_jp3                 = 15;                 // operation mode (JP3 jumper)
const byte    prt_pb0                 = 17;                 // pushbutton 0
const byte    prt_pb1                 = 18;                 // pushbutton 1
const byte    prt_pb2                 = 19;                 // pushbutton 2
const byte    prt_pb3                 = 20;                 // pushbutton 3
const byte    prt_pb4                 = 21;                 // pushbutton 4
const byte    prt_pb5                 = 22;                 // pushbutton 5
const byte    prt_led                 = LED_BUILTIN;        // LED on the board of Pico
byte          uparrow[8]              = {B00100,            // up arrow character for LCD
                                         B01110,
                                         B10101,
                                         B00100,
                                         B00100,
                                         B00100,
                                         B00100
                                        };
byte          downarrow[8]            = {B00100,            // down arrow character for LCD
                                         B00100,
                                         B00100,
                                         B00100,
                                         B10101,
                                         B01110,
                                         B00100
                                        };
// general constants
const String  swversion               = "0.1";              // version of this program
const int     btn_delay               = 200;                // time after read button status
// general variables
byte          virtoverridepagenum     = 0;                  // page num. for copy data (virtstatuspage->display)
byte          virtstatuspage[9][10];                        // virtual status pages
byte          virtstatuspagenum       = 0;                  // page num. for copy data (virtstatuspage->display)
char          virtscreen[virtscreenxsize][virtscreenysize]; // virtual screen
byte          virtscreenline          = 0;                  // y pos. for copy data (rxdbuffer->virtscreen)
byte          virtscreenxpos          = 0;                  // x pos. for copy data (virtscreen->display)
byte          virtscreenypos          = 0;                  // y pos. for copy data (virtscreen->display)
byte          virtscreenscrolllock    = 0;                  // lock autoscroll of the log
byte          operationmode;                                // operation mode of device
byte          operationsubmode        = 0;                  // sub operation mode of device in mode #3
unsigned long currenttime;                                  // current time
unsigned long previoustime            = 0;                  // last time of receiving or button pressing

// messages
String msg[28]                        = {"    MM8D console",               // 00
                                         "--------------------",           // 01
                                         "sw.: v",                         // 02
                                         "(C)2022 Pozsar Zsolt",           // 03
                                         "Initializing...",                // 04
                                         " * GPIO ports",                  // 05
                                         " * LCD",                         // 06
                                         " * Serial ports:",               // 07
                                         "operation mode: #",              // 08
                                         "Read a line from serial port #", // 09
                                         "Button pressed: PB",             // 10
                                         "Change to status pages",         // 11
                                         "Change to override pages",       // 12
                                         "Change to log page",             // 13
                                         "Lock autoscroll of log page",    // 14
                                         "STATUS",                         // 15
                                         "val",                            // 16
                                         "in",                             // 17
                                         "out",                            // 18
                                         "disabled channel",               // 19
                                         "OVERRIDE",                       // 20
                                         "tube #",                         // 21
                                         "neutral",                        // 22
                                         "    off",                        // 23
                                         "     on",                        // 24
                                         "lamp:      ",                    // 25
                                         "ventilator:",                    // 26
                                         "heater:    "                     // 27
                                        };

#ifdef ARDUINO_ARCH_MBED_RP2040
UART Serial2(com_rxd2, com_txd, NC, NC);
#endif

#ifdef LCD_8BIT
LiquidCrystal lcd(lcd_rs, lcd_en, lcd_db0, lcd_db1, lcd_db2, lcd_db3, lcd_db4, lcd_db5, lcd_db6, lcd_db7);
#else
LiquidCrystal lcd(lcd_rs, lcd_en, lcd_db4, lcd_db5, lcd_db6, lcd_db7);
#endif

void com_writetoconsole(String message) {
#ifdef COM_USB_MESSAGES
  Serial.println(message);
#endif
#ifdef COM_TTL_MESSAGES
  Serial1.println(message);
#endif
#ifdef COM_RS232C_MESSAGES
  Serial2.println(message);
#endif
}

// LCD - turn on/off background light
void lcd_backlight(byte opmode) {
  switch (opmode) {
    case 0:
      digitalWrite(lcd_bl, LOW);
      break;
    case 1:
      digitalWrite(lcd_bl, HIGH);
      break;
    case 2:
      previoustime = millis();
      break;
    case 3:
      currenttime = millis();
      if (currenttime - previoustime >= lcd_bloffinterval) {
        digitalWrite(lcd_bl, LOW);
      } else {
        digitalWrite(lcd_bl, HIGH);
      }
      break;
  }
}

// copy selected virtual status page to LCD
void copyvirtstatuspage2lcd(byte page) {
  if (page == 0) {
    /*
        +--------------------+
        |CH #0  [  ]   STATUS|
        |val   T:00°C        |
        |in    BE:0 LP:0 HP:0|
        |out   T1:0 T2:0 T3:0|
        +--------------------+
    */
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("CH #" + String(page));
    lcd.setCursor(7, 0); lcd.print("[  ]");
    lcd.setCursor(8, 0); lcd.write(byte(0));
    lcd.setCursor(lcd_xsize - msg[15].length(), 0); lcd.print(msg[15]);
    lcd.setCursor(0, 1); lcd.print(msg[16]);
    lcd.setCursor(0, 2); lcd.print(msg[17]);
    lcd.setCursor(0, 3); lcd.print(msg[18]);
    lcd.setCursor(lcd_xsize - 14, 1);
    lcd.print("T:" + String(virtstatuspage[page][3]) + char(0xDF) + "C");
    lcd.setCursor(lcd_xsize - 14, 2);
    lcd.print("BE:" + String(virtstatuspage[page][0]) + " " +
              +"LP:" + String(virtstatuspage[page][1]) + " " +
              +"HP:" + String(virtstatuspage[page][2]));
    byte tube[3];
    for (byte b = 4; b < 7; b++) {
      tube[b - 4] = virtstatuspage[page][b];
      switch (virtstatuspage[page][b]) {
        case 0x02: tube[b - 4] = 0x00;
          break;
        case 0x03: tube[b - 4] = 0x01;
          break;
      }
    }
    lcd.setCursor(lcd_xsize - 14, 3);
    lcd.print("T1:" + String(tube[0]) + " " + "T2:" + String(tube[1]) + " " + "T3:" + String(tube[2]));
  } else {
    if (virtstatuspage[page][3] == 0x7F) {
      /*
          +--------------------+
          |CH #3  [  ]   STATUS|
          |                    |
          |  disabled channel  |
          |                    |
          +--------------------+
      */
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("CH #" + String(page));
      lcd.setCursor(7, 0); lcd.print("[  ]");
      if (page < 8) {
        lcd.setCursor(8, 0); lcd.write(byte(0));
      }
      lcd.setCursor(9, 0); lcd.write(byte(1));
      lcd.setCursor(lcd_xsize - msg[15].length(), 0); lcd.print(msg[15]);
      lcd.setCursor(lcd_xsize / 2 - msg[19].length() / 2, 2); lcd.print(msg[19]);
    } else {
      /*
          +--------------------+
          |CH #1  [  ]   STATUS|
          |val   T:00°C RH:100%|
          |in    OM:H CM:A BE:0|
          |out   LA:0 VE:0 HE:0|
          +--------------------+
      */
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("CH #" + String(page));
      lcd.setCursor(7, 0); lcd.print("[  ]");
      if (page < 8) {
        lcd.setCursor(8, 0); lcd.write(byte(0));
      }
      lcd.setCursor(9, 0); lcd.write(byte(1));
      lcd.setCursor(lcd_xsize - msg[15].length(), 0); lcd.print(msg[15]);
      lcd.setCursor(0, 1); lcd.print(msg[16]);
      lcd.setCursor(0, 2); lcd.print(msg[17]);
      lcd.setCursor(0, 3); lcd.print(msg[18]);
      lcd.setCursor(lcd_xsize - 14, 1);
      lcd.print("T:" + String(virtstatuspage[page][0]) + char(0xDF) + "C " +
                +"RH:" + String(virtstatuspage[page][1]) + "%");
      lcd.setCursor(lcd_xsize - 14, 2);
      String OM;
      String CM;
      OM = "?";
      CM = "?";
      switch (virtstatuspage[page][3]) {
        case 0x00:
          OM = 'H';
          break;
        case 0x01:
          OM = 'M';
          break;
        case 0x7F:
          OM = 'D';
          break;
      }
      switch (virtstatuspage[page][4]) {
        case 0x00:
          CM = 'A';
          break;
        case 0x01:
          CM = 'M';
          break;
      }
      lcd.print("OM:" + OM + " " + "CM:" + CM + " " + "BE:" + String(virtstatuspage[page][5]));
      byte out[3];
      for (byte b = 7; b < 10; b++) {
        out[b - 7] = virtstatuspage[page][b];
        switch (virtstatuspage[page][b]) {
          case 0x02: out[b - 7] = 0x00;
            break;
          case 0x03: out[b - 7] = 0x01;
            break;
        }
      }
      lcd.setCursor(lcd_xsize - 14, 3);
      lcd.print("LA:" + String(out[0]) + " " + "VE:" + String(out[1]) + " " + "HE:" + String(out[2]));
    }
  }
}

// clear virtual status pages
void clearvirtstatuspage() {
  for (byte y = 0; y <= 9; y++) {
    for (byte x = 0; x <= 8; x++) {
      virtstatuspage[x][y] = 0;
    }
  }
}

// copy selected virtual override page to LCD
void copyvirtoverridepage2lcd(byte page) {
  if (page == 0) {
    /*
        +--------------------+
        |CH #0  [  ] OVERRIDE|
        |tube #1:     neutral|
        |tube #2:     neutral|
        |tube #3:     neutral|
        +--------------------+
    */
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("CH #" + String(page));
    lcd.setCursor(7, 0); lcd.print("[  ]");
    lcd.setCursor(8, 0); lcd.write(byte(0));
    lcd.setCursor(lcd_xsize - msg[20].length(), 0); lcd.print(msg[20]);
    for (byte b = 1; b < 4; b++) {
      lcd.setCursor(0, b); lcd.print(msg[21] + String(b) + ":");
      lcd.setCursor(lcd_xsize - msg[22].length(), b);
      switch (virtstatuspage[page][b + 3]) {
        case 0x00: lcd.print(msg[22]);
          break;
        case 0x01: lcd.print(msg[22]);
          break;
        case 0x02: lcd.print(msg[23]);
          break;
        case 0x03: lcd.print(msg[24]);
          break;
      }
    }
  } else {
    /*
        +--------------------+
        |CH #1       OVERRIDE|
        |lamp:             on|
        |ventilator:  neutral|
        |heater:          off|
        +--------------------+
    */
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("CH #" + String(page));
    lcd.setCursor(7, 0); lcd.print("[  ]");
    if (page < 8) {
      lcd.setCursor(8, 0); lcd.write(byte(0));
    }
    lcd.setCursor(9, 0); lcd.write(byte(1));
    lcd.setCursor(lcd_xsize - msg[20].length(), 0); lcd.print(msg[20]);
    for (byte b = 1; b < 4; b++) {
      lcd.setCursor(0, b); lcd.print(msg[24 + b]);
      lcd.setCursor(lcd_xsize - msg[22].length(), b);
      switch (virtstatuspage[page][b + 6]) {
        case 0x00: lcd.print(msg[22]);
          break;
        case 0x01: lcd.print(msg[22]);
          break;
        case 0x02: lcd.print(msg[23]);
          break;
        case 0x03: lcd.print(msg[24]);
          break;
      }
    }
  }
}

// scroll up one line on virtual screen (log page in Mode #3)
void scroll(byte lastline) {
  if (virtscreenscrolllock == 0 ) {
    for (byte y = 1; y <= lastline; y++) {
      for (byte x = 0; x <= virtscreenxsize - 1; x++) {
        virtscreen[x][y - 1] = virtscreen[x][y];
        if (y == lastline) {
          virtscreen[x][y] = SPACE;
        }
      }
    }
  }
}

// copy text from virtual screen (log page in Mode #3) to LCD
void copyvirtscreen2lcd(byte x, byte y) {
  for (byte dy = 0; dy <= lcd_ysize - 1; dy++) {
    for (byte dx = 0; dx <= lcd_xsize - 1; dx++) {
      lcd.setCursor(dx, dy);
      lcd.write(virtscreen[x + dx][y + dy]);
    }
  }
}

// clear virtual screen (log page in Mode #3)
void clearvirtscreen() {
  for (byte y = 0; y <= virtscreenysize - 1; y++) {
    for (byte x = 0; x <= virtscreenxsize - 1; x++) {
      virtscreen[x][y] = SPACE;
    }
  }
}

// read communication port and move data to virtual screen
byte com_handler(byte port) {
  const byte rxdbuffersize       = 255;
  boolean newpage                = false;
  byte lastline;
  char rxdbuffer[rxdbuffersize];
  int rxdlength                  = 0;
  // read port and write to receive buffer
  switch (port) {
    case 0:
      if (Serial.available()) {
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial.readBytes(rxdbuffer, rxdbuffersize);
        com_writetoconsole(msg[9] + String(port));
        digitalWrite(prt_led, LOW);
        // lcd_backlight(2);
      }
      break;
    case 1:
      if (Serial1.available()) {
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial1.readBytes(rxdbuffer, rxdbuffersize);
        com_writetoconsole(msg[9] + String(port));
        digitalWrite(prt_led, LOW);
        // lcd_backlight(2);
      }
      break;
    case 2:
      if (Serial2.available()) {
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial2.readBytes(rxdbuffer, rxdbuffersize);
        com_writetoconsole(msg[9] + String(port));
        digitalWrite(prt_led, LOW);
        // lcd_backlight(2);
      }
      break;
  }
  lcd_backlight(3);
  // check datalenght
  if (rxdlength > virtscreenxsize) {
    rxdlength = virtscreenxsize;
  }
  // copy line from receive buffer to virtual screen
  if (rxdlength) {
    switch (operationmode) {
      // in Mode #0
      case 0:
        lastline = 3;
        if (virtscreenline == lastline + 1) {
          scroll(lastline);
          virtscreenline = lastline;
        }
        for (byte b = 0; b < rxdlength; b++) {
          if (rxdbuffer[b] == BEL) {
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
          }
          if (rxdbuffer[b] == TAB) {
            rxdbuffer[b] = SPACE;
          }
          if ((rxdbuffer[b] >= SPACE) and (rxdbuffer[b] < DEL)) {
            virtscreen[b][virtscreenline] = rxdbuffer[b];
          }
        }
        virtscreenline++;
        copyvirtscreen2lcd(virtscreenxpos, 0);
        break;
      // in Mode #1
      case 1:
        lastline = virtscreenysize - 1;
        if (virtscreenline == lastline + 1) {
          scroll(lastline);
          virtscreenline = lastline;
        }
        for (byte b = 0; b < rxdlength; b++) {
          if (rxdbuffer[b] == BEL) {
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
          }
          if (rxdbuffer[b] == TAB) {
            rxdbuffer[b] = SPACE;
          }
          if ((rxdbuffer[b] >= SPACE) and (rxdbuffer[b] < DEL)) {
            virtscreen[b][virtscreenline] = rxdbuffer[b];
          }
        }
        virtscreenline++;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        break;
      // in Mode #2
      case 2:
        lastline = virtscreenysize - 1;
        if (virtscreenline == lastline + 1) {
          virtscreenline = 0;
        }
        for (byte b = 0; b < rxdlength; b++) {
          if (rxdbuffer[b] == BEL) {
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
            delay(250);
            lcd_backlight(0);
            delay(250);
            lcd_backlight(1);
          }
          if (rxdbuffer[b] == TAB) {
            rxdbuffer[b] == SPACE;
          }
          if (rxdbuffer[b] == FF) {
            newpage = true;
          }
          if ((rxdbuffer[b] >= SPACE) and (rxdbuffer[b] < DEL)) {
            virtscreen[b][virtscreenline] = rxdbuffer[b];
          }
        }
        if (newpage) {
          clearvirtscreen();
          virtscreenline = 0;
          newpage = false;
        } else {
          virtscreenline++;
        }
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        break;
      case 3:
        // in Mode #3
        if ((rxdbuffer[0] == 0x43) and (rxdbuffer[1] == 0x48)) // "CH"
        {
          for (byte b = 3; b < 13; b++) {
            virtstatuspage[rxdbuffer[2]][b - 3] = rxdbuffer[b];
          }
          if (operationsubmode < 2) {
            switch (operationsubmode) {
              case 0:
                copyvirtstatuspage2lcd(virtstatuspagenum);
                break;
              case 1:
                copyvirtoverridepage2lcd(virtoverridepagenum);
                break;
            }
          }
        } else
        {
          // if the received line is log
          lastline = virtscreenysize - 1;
          if (virtscreenline == lastline + 1) {
            scroll(lastline);
            virtscreenline = lastline;
          }
          for (byte b = 0; b < rxdlength; b++) {
            if (rxdbuffer[b] == BEL) {
              lcd_backlight(0);
              delay(250);
              lcd_backlight(1);
              delay(250);
              lcd_backlight(0);
              delay(250);
              lcd_backlight(1);
              delay(250);
              lcd_backlight(0);
              delay(250);
              lcd_backlight(1);
            }
            if (rxdbuffer[b] == TAB) {
              rxdbuffer[b] = SPACE;
            }
            if ((rxdbuffer[b] >= SPACE) and (rxdbuffer[b] < DEL)) {
              virtscreen[b][virtscreenline] = rxdbuffer[b];
            }
          }
          virtscreenline++;
          if (operationsubmode == 2) {
            copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          }
        }
        break;
    }
  }
  return rxdlength;
}

// read status of pushbuttons and write text to LCD
void btn_handler(byte m, byte sm) {
  // horizontal move
  if ((m >= 0) and (m < 3)) {
    // [LEFT] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb0)) {
      com_writetoconsole(msg[10] + "0");
      delay(btn_delay);
      lcd_backlight(2);
      // move lines
      if (virtscreenxpos > 0) {
        virtscreenxpos--;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
    // [RIGHT] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb1)) {
      com_writetoconsole(msg[10] + "1");
      delay(btn_delay);
      lcd_backlight(2);
      // move lines
      if (virtscreenxpos + lcd_xsize < virtscreenxsize) {
        virtscreenxpos++;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
  }
  if (m == 3) {
    // [LEFT] button in Mode #3
    if (not digitalRead(prt_pb0)) {
      com_writetoconsole(msg[10] + "0");
      delay(btn_delay);
      lcd_backlight(2);
      switch (sm) {
        case 0:
          // SubMode #0: nothing
          break;
        case 1:
          // SubMode #1: nothing
          break;
        case 2:
          // SubMode #2: move lines
          if (virtscreenxpos > 0) {
            virtscreenxpos--;
            copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          }
          break;
      }
    }
    // [RIGHT] button in Mode #3
    if (not digitalRead(prt_pb1)) {
      com_writetoconsole(msg[10] + "1");
      delay(btn_delay);
      lcd_backlight(2);
      switch (sm) {
        case 0:
          // SubMode #0: nothing
          break;
        case 1:
          // SubMode #1: nothing
          break;
        case 2:
          // SubMode #2: move lines
          if (virtscreenxpos + lcd_xsize < virtscreenxsize) {
            virtscreenxpos++;
            copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          }
          break;
      }
    }
  }
  // vertical move
  if ((m > 0) and (m < 3)) {
    // [UP] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb2)) {
      com_writetoconsole(msg[10] + "2");
      delay(btn_delay);
      lcd_backlight(2);
      // scroll lines
      if (virtscreenypos > 0) {
        virtscreenypos--;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
    // [DOWN] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb3)) {
      com_writetoconsole(msg[10] + "3");
      delay(btn_delay);
      lcd_backlight(2);
      // scroll lines
      if (virtscreenypos + lcd_ysize < virtscreenysize) {
        virtscreenypos++;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
  }
  if (m == 3) {
    // [UP] button in Mode #3
    if (not digitalRead(prt_pb2)) {
      com_writetoconsole(msg[10] + "2");
      delay(btn_delay);
      lcd_backlight(2);
      switch (sm) {
        case 0:
          // SubMode #0: change page
          if (virtstatuspagenum > 0) {
            virtstatuspagenum--;
            copyvirtstatuspage2lcd(virtstatuspagenum);
          }
          break;
        case 1:
          // SubMode #1: change page
          if (virtoverridepagenum > 0) {
            virtoverridepagenum--;
            copyvirtoverridepage2lcd(virtoverridepagenum);
          }
          break;
        case 2:
          // SubMode #2: scroll lines
          if (virtscreenypos > 0) {
            virtscreenypos--;
            copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          }
          break;
      }
    }
    // [DOWN] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb3)) {
      com_writetoconsole(msg[10] + "3");
      delay(btn_delay);
      lcd_backlight(2);
      switch (sm) {
        case 0:
          // SubMode #0: change page
          if (virtstatuspagenum < 8) {
            virtstatuspagenum++;
            copyvirtstatuspage2lcd(virtstatuspagenum);
          }
          break;
        case 1:
          // SubMode #1: change page
          if (virtoverridepagenum < 8) {
            virtoverridepagenum++;
            copyvirtoverridepage2lcd(virtoverridepagenum);
          }
          break;
        case 2:
          // SubMode #2: scroll lines
          if (virtscreenypos + lcd_ysize < virtscreenysize) {
            virtscreenypos++;
            copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          }
          break;
      }
    }
  }
  // function buttons
  if (m == 3) {
    // [F1] button in Mode #3
    if (not digitalRead(prt_pb4)) {
      com_writetoconsole(msg[10] + "4");
      delay(btn_delay);
      lcd_backlight(2);
      // change SubMode and view actual page
      if (operationsubmode < 2) {
        operationsubmode++;
      } else {
        operationsubmode = 0;
      }
      lcd.clear();
      switch (operationsubmode) {
        case 0:
          copyvirtstatuspage2lcd(virtstatuspagenum);
          com_writetoconsole(msg[11]);
          break;
        case 1:
          copyvirtoverridepage2lcd(virtoverridepagenum);
          com_writetoconsole(msg[12]);
          break;
        case 2:
          virtscreenscrolllock = 0;
          copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
          com_writetoconsole(msg[13]);
          break;
      }
    }
    // [F2] button in Mode #3
    if (not digitalRead(prt_pb5)) {
      com_writetoconsole(msg[10] + "5");
      delay(btn_delay);
      lcd_backlight(2);
      // SubMode #2: lock autoscroll of the log
      if (operationsubmode == 2) {
        virtscreenscrolllock = not virtscreenscrolllock;
      }
      if (virtscreenscrolllock == 1) {
        com_writetoconsole(msg[14]);
      }
    }
  }
}
// get operation mode of device
byte getmode() {
  delay(250);
  return ((not digitalRead(prt_jp3)) * 2 + (not digitalRead(prt_jp2)));
}

// * * * MAIN FUNCTION * * *

// initializing
void setup() {
  String s;
  // set USB serial port
  delay(3000);
  Serial.begin(com_speed[0]);
  Serial1.begin(com_speed[1]);
  Serial2.begin(com_speed[2]);
  // write program information to console
  for (int b = 0; b <= 4; b++) {
    if (b == 2) {
      com_writetoconsole(msg[b] + swversion);
    } else {
      com_writetoconsole(msg[b]);
    }
  }
  // initializing I/O devices
  // GPIO ports
  com_writetoconsole(msg[5]);
  pinMode(lcd_bl, OUTPUT);
  pinMode(lcd_db0, OUTPUT);
  pinMode(lcd_db1, OUTPUT);
  pinMode(lcd_db2, OUTPUT);
  pinMode(lcd_db3, OUTPUT);
  pinMode(lcd_db4, OUTPUT);
  pinMode(lcd_db5, OUTPUT);
  pinMode(lcd_db6, OUTPUT);
  pinMode(lcd_db7, OUTPUT);
  pinMode(lcd_en, OUTPUT);
  pinMode(lcd_rs, OUTPUT);
  pinMode(prt_jp2, INPUT);
  pinMode(prt_jp3, INPUT);
  pinMode(prt_led, OUTPUT);
  pinMode(prt_pb0, INPUT);
  pinMode(prt_pb1, INPUT);
  pinMode(prt_pb2, INPUT);
  pinMode(prt_pb3, INPUT);
  pinMode(prt_pb4, INPUT);
  pinMode(prt_pb5, INPUT);
  // display
  com_writetoconsole(msg[6]);
  lcd.createChar(0, uparrow);
  lcd.createChar(1, downarrow);
  lcd.begin(lcd_xsize, lcd_ysize);
  lcd_backlight(1);
  lcd_backlight(2);
  // write program information to display
  for (int b = 0; b <= 3; b++) {
    lcd.setCursor(0, b);
    if (b == 2) {
      lcd.print(msg[b] + swversion);
    } else {
      lcd.print(msg[b]);
    }
  }
  delay(3000);
  lcd.clear();
  // serial ports
  com_writetoconsole(msg[7]);
  Serial1.begin(com_speed[1]);
  Serial2.begin(com_speed[2]);
  for (int b = 0; b <= 2; b++) {
    s = "#" + String(b) + ": " + String(com_speed[b]) + " baud";
    com_writetoconsole("   " + s);
    lcd.setCursor(0, b); lcd.print(s);
  }
  // get operation mode
  operationmode = getmode();
  s = msg[8] + String(operationmode);
  com_writetoconsole(" * " + s);
  lcd.setCursor(0, 3); lcd.print(s);
  delay(3000);
  lcd.clear();
  // clean of fill data virtual screen and copy LCD
#ifdef TEST
  char ch = 33;
  for (byte y = 0; y <= virtscreenysize - 1; y++) {
    for (byte x = 0; x <= virtscreenxsize - 1; x++) {
      if (y % 2 == 0) {
        virtscreen[x][y] = ch + x;
      } else {
        virtscreen[x][y] = ch + x + 1;
      }
    }
  }
  for (byte y = 0; y <= virtscreenysize - 1; y++) {
    virtscreen[0][y] = 48 + (y % 10);
  }
#else
  clearvirtscreen();
  clearvirtstatuspage();
#endif
  if (operationmode == 3) {
    copyvirtstatuspage2lcd(0);
  } else {
    copyvirtscreen2lcd(0, 0);
  }
}

// main function
void loop() {
  String s;
  // auto LCD backlight off
  lcd_backlight(3);
  // get operation mode
  if (getmode() != operationmode) {
    operationmode = getmode();
    s = msg[8] + String(operationmode);
    com_writetoconsole(" * " + s);
    lcd.clear();
    lcd.setCursor(0, 2); lcd.print(s);
    delay(1000);
    lcd.clear();
  }
  // handling serial ports
#ifdef COM_USB
  com_handler(0);
#endif
#ifdef COM_TTL
  com_handler(1);
#endif
#ifdef COM_RS232C
  com_handler(2);
#endif
  // handling buttons
  btn_handler(operationmode, operationsubmode);
}
