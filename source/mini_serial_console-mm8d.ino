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
  Operation modes:

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
               automatically scrolling lines,
               the displayed page can be change with pushbuttons,
               switch to other submode with PB5 button.
    Mode #3/1: read only mode,
               no cursor,
               20x4 size displayed area on 80x25* size virtual screen,
               automatically scrolling lines,
               the displayed area can be moved horizontally and vertically with
               pushbuttons,
               switch to other submode with PB4 button.

    Note:
     '*': You can set size of the virtual screen with virtscreenxsize and
          virtscreenysize constants in operation mode #1 and #2.


  Button functions:

    Button  Mode #0     Mode #1     Mode #2     Mode #3/0   Mode #3/1
    -------------------------------------------------------------------
     PB0    move left   move left   move left               move left
     PB1    move right  move right  move right              move right
     PB2                move up     move up     page up     scroll up
     PB3                move down   move down   page down   scroll down
     PB4                                        view status view status
     PB5                                        view log    view log

  Serial ports:

    Serial0: via USB
    Serial1: 3.3V TTL  UART0
    Serial2: RS232C    UART1

  Example incoming lines in Mode #3:

      1  2 3 4 5  6 7 8
    --------------------
    "CH0|0|0|0|31|0|0|0"

    1:  channel
    2:  overcurrent breaker error             0: closed  1: opened
    3:  water pump pressure error (no water)  0: good    1: bad
    4:  water pump pressure error (clogging)  0: good    1: bad
    5:  external temperature in °C
    6:  status of water pump and tube #1      0: off     1: on
    7:  status of water pump and tube #2      0: off     1: on
    8:  status of water pump and tube #3      0: off     1: on

      1  2  3  4  5 6 7 8 9 a b
    ----------------------------
    "CH1|11|21|31|0|1|0|0|1|0|1"

    1:  channel
    2:  temperature in °C
    3:  relative humidity
    4:  relative unwanted gas concentrate
    5:  operation mode                        0: hyphae  1: mushroom
    6:  manual mode                           0: auto    1: manual
    7:  overcurrent breaker error             0: closed  1: opened
    8:  status of door (alarm)                0: closed  1: opened
    9:  status of lamp output                 0: off     1: on
    a:  status of ventilator output           0: off     1: on
    b:  status of heater output               0: off     1: on

      1  2
    ----------------------------
    "CH8|DISABLED"

    1:  channel
    2:  sign of disabled channel

       1      2    3           4
    ----------------------------------------------------------
    "221213 114421 I Configuration is loaded."
    "221213 114427 W CH2: MM6D is not accessible."
    "221213 114427 E ERROR #18: There is not enabled channel!"

    1:  date in yymmdd format
    2:  time in hhmmss format
    3:  level (information, warning, error)
    4:  short description
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
const int     lcd_bloffinterval = 60000;                    // LCD backlight off time after last button press
const byte    lcd_xsize         = 20;                       // horizontal size of display
const byte    lcd_ysize         = 4;                        //vertical size of display
const byte    virtscreenxsize   = 80;                       // horizontal size of virtual screen
const byte    virtscreenysize   = 25;                       // vertical size of virtual screen
// serial ports
const int     com_speed[3]      = {115200, 9600, 9600};     // speed of the USB serial port
#ifdef ARDUINO_ARCH_MBED_RP2040
const byte    com_rxd2          = 8;
const byte    com_txd2          = 9;
#endif
// GPIO ports
const byte    lcd_bl            = 14;                       // LCD - backlight on/off
const byte    lcd_db0           = 2;                        // LCD - databit 0
const byte    lcd_db1           = 3;                        // LCD - databit 1
const byte    lcd_db2           = 4;                        // LCD - databit 2
const byte    lcd_db3           = 5;                        // LCD - databit 3
const byte    lcd_db4           = 10;                       // LCD - databit 4
const byte    lcd_db5           = 11;                       // LCD - databit 5
const byte    lcd_db6           = 12;                       // LCD - databit 6
const byte    lcd_db7           = 13;                       // LCD - databit 7
const byte    lcd_en            = 7;                        // LCD - enable
const byte    lcd_rs            = 6;                        // LCD - register select
const byte    prt_jp2           = 16;                       // operation mode (JP2 jumper)
const byte    prt_jp3           = 15;                       // operation mode (JP3 jumper)
const byte    prt_pb0           = 17;                       // pushbutton 0
const byte    prt_pb1           = 18;                       // pushbutton 1
const byte    prt_pb2           = 19;                       // pushbutton 2
const byte    prt_pb3           = 20;                       // pushbutton 3
const byte    prt_pb4           = 21;                       // pushbutton 4
const byte    prt_pb5           = 22;                       // pushbutton 5
const byte    prt_led           = LED_BUILTIN;              // LED on the board of Pico
// general constants
const String  swversion         = "0.1";                    // version of this program
const int     btn_delay         = 200;                      // time after read button status
// general variables
char          virtstatuspage[9][10];                        // virtual status pages
byte          virtstatuspagenum = 0;                        // page num. for copy data (virtstatuspage->display)
char          virtscreen[virtscreenxsize][virtscreenysize]; // virtual screen
byte          virtscreenline    = 0;                        // y pos. for copy data (rxdbuffer->virtscreen)
byte          virtscreenxpos    = 0;                        // x pos. for copy data (virtscreen->display)
byte          virtscreenypos    = 0;                        // y pos. for copy data (virtscreen->display)
byte          operationmode;                                // operation mode of device
byte          operationsubmode  = 0;                        // sub operation mode of device in mode #3
unsigned long currenttime;                                  // current time
unsigned long previoustime      = 0;                        // last time of receiving or button pressing

// messages
String msg[14]                  =
{
  /*  0 */  "    MM8D console",
  /*  1 */  "--------------------",
  /*  2 */  "sw.: v",
  /*  3 */  "(C)2022 Pozsar Zsolt",
  /*  4 */  "Initializing...",
  /*  5 */  " * GPIO ports",
  /*  6 */  " * LCD",
  /*  7 */  " * Serial ports:",
  /*  8 */  "Operation mode: #",
  /*  9 */  "Read a line from serial port #",
  /* 10 */  "Button pressed: PB"
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

// scroll up one line on virtual screen
void scroll(byte lastline) {
  for (byte y = 1; y <= lastline; y++) {
    for (byte x = 0; x <= virtscreenxsize - 1; x++) {
      virtscreen[x][y - 1] = virtscreen[x][y];
      if (y == lastline) {
        virtscreen[x][y] = SPACE;
      }
    }
  }
}

// copy text from virtual screen to LCD
void copyvirtscreen2lcd(byte x, byte y) {
  for (byte dy = 0; dy <= lcd_ysize - 1; dy++) {
    for (byte dx = 0; dx <= lcd_xsize - 1; dx++) {
      lcd.setCursor(dx, dy);
      lcd.write(virtscreen[x + dx][y + dy]);
    }
  }
}

// clear virtual screen
void clearvirtscreen() {
  for (byte y = 0; y <= virtscreenysize - 1; y++) {
    for (byte x = 0; x <= virtscreenxsize - 1; x++) {
      virtscreen[x][y] = SPACE;
    }
  }
}

// copy selected virtual status page to LCD
void copyvirtstatuspage2lcd(byte page) {
  // !!! ne felejts el !!!
}

// clear virtual status pages
void clearvirtstatuspage() {
  for (byte y = 0; y <= 9; y++) {
    for (byte x = 0; x <= 8; x++) {
      virtstatuspage[x][y] = SPACE;
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
        if ((rxdbuffer[0] == 0x43) and (rxdbuffer[1] == 0x48))
        {
          // if the received line is data
          // !!! tárolás !!!
          if (operationsubmode == 0) {
            // !!! aktuális oldal megjelenítése !!!
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
          if (operationsubmode == 1) {
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
      // SubMode #0: nothing
      //         #1: scroll lines
      if (sm == 0) {
      } else {
        if (virtscreenxpos > 0) {
          virtscreenxpos--;
          copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        }
      }
    }
    // [RIGHT] button in Mode #3
    if (not digitalRead(prt_pb1)) {
      com_writetoconsole(msg[10] + "1");
      delay(btn_delay);
      lcd_backlight(2);
      // SubMode #0: nothing
      //         #1: scroll lines
      if (sm == 0) {
      } else {
        if (virtscreenxpos + lcd_xsize < virtscreenxsize) {
          virtscreenxpos++;
          copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        }
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
      // SubMode #0: change page
      //         #1: scroll lines
      if (sm == 0) {
        // !!! előző oldal megjelenítése !!!
      } else {
        if (virtscreenypos > 0) {
          virtscreenypos--;
          copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        }
      }
    }
    // [DOWN] button in Mode #0, #1 and #2
    if (not digitalRead(prt_pb3)) {
      com_writetoconsole(msg[10] + "3");
      delay(btn_delay);
      lcd_backlight(2);
      // SubMode #0: change page
      //         #1: scroll lines
      if (sm == 0) {
        // !!! következő oldal megjelenítése !!!
      } else {
        if (virtscreenypos + lcd_ysize < virtscreenysize) {
          virtscreenypos++;
          copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        }
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
      // change to SubMode #0, and view actual page
      operationsubmode = 0;
      lcd.clear();
      // !!! aktuális oldal megjelenítése !!!
    }
    // [F2] button in Mode #3
    if (not digitalRead(prt_pb5)) {
      com_writetoconsole(msg[10] + "5");
      delay(btn_delay);
      lcd_backlight(2);
      // change to SubMode #1, and view log at actual position
      operationsubmode = 1;
      lcd.clear();
      copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
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
    s = "#" + String(b) + ": " + String(com_speed[b]) + " b/s";
    com_writetoconsole("   " + s);
    lcd.setCursor(0, b);
    lcd.print(s);
  }
  // get operation mode
  operationmode = getmode();
  s = msg[8] + String(operationmode);
  com_writetoconsole(" * " + s);
  lcd.setCursor(0, 3);
  lcd.print(s);
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
  lcd_backlight(2);
  lcd_backlight(3);
  // get operation mode
  if (getmode() != operationmode) {
    operationmode = getmode();
    s = msg[8] + String(operationmode);
    com_writetoconsole(" * " + s);
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print(s);
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
