// +---------------------------------------------------------------------------+
// | Mini serial console * v0.1                                                |
// | Copyright (C) 2022 Pozsár Zsolt <pozsarzs@gmail.com>                      |
// | mini_serial_console.ino                                                   |
// | Program for Raspberry Pi Pico                                             |
// +---------------------------------------------------------------------------+

/*
      This program is free software: you can redistribute it and/or modify it
    under the terms of the European Union Public License 1.2 version.

      This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.
*/

/* Operation modes:

    Mode #0: read only mode,
             no cursor,
             20x4 size displayed area on 80x4* size virtual screen,
             automatically scrolling lines,
             the displayed area can be moved horizontally with pushbuttons.
    Mode #1: read only mode,
             no cursor,
             20x4 size displayed area on 80x25* size virtual screen,
             automatically scrolling lines,
             the displayed area can be moved horizontally and vertically with pushbuttons.
    Mode #2: read only mode,
             no cursor,
             20x4 size displayed area on 80x25* size virtual screen,
             the displayed area can be moved horizontally and vertically with pushbuttons,
             after FormFeed (0x12), a new, clean page starts.
    Mode #3: reserved for device depend solutions (menu, read/write mode etc.)

    Note:
     '*': You can set size of the virtual screen with virtscreenxsize and virtscreenysize constants.


   Button functions:

    Button  Mode #0    Mode #1    Mode #2    Mode #3
    ---------------------------------------------------
    PB0     move left  move left  move left  move left
    PB1     move right move right move right move right
    PB2                move up    move up    move up
    PB3                move down  move down  move down
    PB4                                      enter
    PB5                                      escape


   Serial ports:

    Serial0: USB    console
    Serial1: UART0  first input   3.3V TTL
    Serial2: UART1  second input  RS232C
*/

#define lcd_4bitmode
#define TEST

#include <LiquidCrystal.h>

// settings
const byte    displayxsize      = 20;                       // horizontal size of display
const byte    displayysize      = 4;                        //vertical size of display
const byte    virtscreenxsize   = 80;                       // horizontal size of virtual screen
const byte    virtscreenysize   = 25;                       // vertical size of virtual screen
const int     bloffinterval     = 60000;                    // LCD backlight off time after last button press
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
char          virtscreen[virtscreenxsize][virtscreenysize]; // virtual screen
byte          virtscreenline    = 0;                        // y pos. for copy data (rxdbuffer->virtscreen)
byte          virtscreenxpos    = 0;                        // x pos. for copy data (virtscreen->display)
byte          virtscreenypos    = 0;                        // y pos. for copy data (virtscreen->display)
byte          operationmode;                                // operation mode of deviceö
unsigned long currenttime;                                  // current time
unsigned long previoustime      = 0;                        // last time of receiving or button pressing

// messages
String msg[14]                  =
{
  /*  0 */  "Mini serial console",
  /*  1 */  "-------------------",
  /*  2 */  "sw.: v",
  /*  3 */  "(C)2022 Pozsar Zsolt",
  /*  4 */  "Initializing...",
  /*  5 */  " * GPIO ports",
  /*  6 */  " * LCD",
  /*  7 */  " * Serial ports:",
  /*  8 */  "Operation mode: #",
  /*  9 */  "Read a line from serial port #",
  /* 10 */  " * I read ",
  /* 11 */  " * Line too long, cut to ",
  /* 12 */  " * Copy line to this y position of the virtual screen: ",
  /* 13 */  "Button pressed: PB"
};

#ifdef ARDUINO_ARCH_MBED_RP2040
UART Serial2(com_rxd2, com_txd, NC, NC);
#endif
#ifdef lcd_4bitmode
LiquidCrystal lcd(lcd_rs, lcd_en, lcd_db4, lcd_db5, lcd_db6, lcd_db7);
#else
LiquidCrystal lcd(lcd_rs, lcd_en, lcd_db0, lcd_db1, lcd_db2, lcd_db3, lcd_db4, lcd_db5, lcd_db6, lcd_db7);
#endif

// LCD - turn on/off background light
void lcd_backlight(byte bl) {
  if (bl) {
    digitalWrite(lcd_bl, HIGH);
  } else {
    digitalWrite(lcd_bl, LOW);
  }
}

// scroll up one line on virtual screen
void scrollup(byte ll) {
  for (byte y = 1; y < (ll + 1); y++) {
    for (byte x = 0; x < virtscreenxsize - 1; x++) {
      if (y < (ll + 1)) {
        virtscreen[x][y] = virtscreen[x][y - 1];
      }
      else {
        virtscreen[x][y - 1] = 20;
      }
    }
  }
}

// read communication port and move data to virtual screen
byte com_handler(byte p) {
  const byte rxdbuffersize = 255;
  char rxdbuffer[rxdbuffersize];
  int rxdlength = 0;
  byte lastline;
  switch (p) {
    case 1:
      if (Serial1.available()) {
        Serial.println(msg[9] + String(p));
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial1.readBytes(rxdbuffer, rxdbuffersize);
        Serial.print(msg[10] + String(rxdlength) + " character(s).");
        digitalWrite(prt_led, LOW);
        previoustime = millis();
      }
      break;
    case 2:
      if (Serial2.available()) {
        Serial.println(msg[9] + String((byte)p));
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial2.readBytes(rxdbuffer, rxdbuffersize);
        Serial.print(msg[10] + String((byte)rxdlength) + " character(s).");
        digitalWrite(prt_led, LOW);
        previoustime = millis();
      }
      break;
  }
  if (rxdlength > virtscreenxsize) {
    rxdlength = virtscreenxsize;
    Serial.print(msg[11] + String((byte)virtscreenxsize) + " characters.");
  }
  if (rxdlength) {
    switch (operationmode) {
      case 0:
        lastline = 3;
        if (virtscreenypos == lastline + 1) {
          scrollup(lastline);
        }
        Serial.print(msg[12] + String(virtscreenypos));
        for (byte b = 0; b < rxdlength; b++) {
          virtscreen[b][virtscreenypos] = rxdbuffer[b];
        }
        if (virtscreenypos < lastline + 1) {
          virtscreenypos++;
        }
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
        break;
      case 1:
        lastline = 24;
        if (virtscreenypos == lastline + 1) {
          scrollup(lastline);
        }
        Serial.print(msg[12] + String((byte)virtscreenypos));
        for (byte b = 0; b < rxdlength; b++) {
          virtscreen[b][virtscreenypos] = rxdbuffer[b];
        }
        if (virtscreenypos <= lastline) {
          virtscreenypos++;
        } else {
          virtscreenypos = lastline + 1;
        }
        break;
      case 2:
        lastline = 24;
        if (virtscreenypos == lastline + 1) {
          virtscreenypos = 0;
        }
        Serial.print(msg[12] + String((byte)virtscreenypos));
        for (byte b = 0; b < rxdlength; b++) {
          if (rxdbuffer[b] == 0x12) {
            virtscreenypos = 0;
          } else {
            virtscreen[b][virtscreenypos] = rxdbuffer[b];
          }
        }
        if (virtscreenypos <= lastline) {
          virtscreenypos++;
        } else {
          virtscreenypos = lastline + 1;
        }
        break;
      case 3:
        // reserved for device depend solutions
        break;
    }
  }
  return rxdlength;
}

// read status of pushbuttons and write text to LCD
void btn_handler(byte m) {
  // horizontal move
  if (not digitalRead(prt_pb0)) {
    Serial.println(msg[13] + "0");
    delay(btn_delay);
    previoustime = millis();
    if (virtscreenxpos > 0) {
      virtscreenxpos--;
      copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
    }
  }
  if (not digitalRead(prt_pb1)) {
    Serial.println(msg[13] + "1");
    delay(btn_delay);
    previoustime = millis();
    if (virtscreenxpos + displayxsize < virtscreenxsize) {
      virtscreenxpos++;
      copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
    }
  }
  // vertical move
  if (m > 0) {
    if (not digitalRead(prt_pb2)) {
      Serial.println(msg[13] + "2");
      delay(btn_delay);
      previoustime = millis();
      if (virtscreenypos > 0) {
        virtscreenypos--;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
    if (not digitalRead(prt_pb3)) {
      Serial.println(msg[13] + "3");
      delay(btn_delay);
      previoustime = millis();
      if (virtscreenypos + displayysize < virtscreenysize) {
        virtscreenypos++;
        copyvirtscreen2lcd(virtscreenxpos, virtscreenypos);
      }
    }
  }
  // data input
  if (m == 3) {
    if (not digitalRead(prt_pb4)) {
      Serial.println(msg[13] + "4");
      delay(btn_delay);
      previoustime = millis();
      // reserved for device depend solutions
    }
    if (not digitalRead(prt_pb5)) {
      Serial.println(msg[13] + "5");
      delay(btn_delay);
      previoustime = millis();
      // reserved for device depend solutions
    }
  }
}

// copy text from virtual screen to LCD
void copyvirtscreen2lcd(byte x, byte y) {
  for (byte dy = 0; dy <= displayysize - 1; dy++) {
    for (byte dx = 0; dx <= displayxsize - 1; dx++) {
      lcd.setCursor(dx, dy);
      lcd.write(virtscreen[x + dx][y + dy]);
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
  // write program information to console
  for (int b = 0; b <= 4; b++) {
    if (b == 2) {
      Serial.println(msg[b] + swversion);
    } else {
      Serial.println(msg[b]);
    }
  }
  // initializing I/O devices
  // GPIO ports
  Serial.println(msg[5]);
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
  Serial.println(msg[6]);
  lcd.begin(displayxsize, displayysize);
  lcd_backlight(1);
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
  Serial.println(msg[7]);
  Serial1.begin(com_speed[1]);
  Serial2.begin(com_speed[2]);
  for (int b = 0; b <= 2; b++) {
    s = "#" + String(b) + ": " + String(com_speed[b]) + " b/s";
    Serial.println("   " + s);
    lcd.setCursor(0, b);
    lcd.print(s);
  }
  // get operation mode
  operationmode = getmode();
  s = msg[8] + String(operationmode);
  Serial.println(" * " + s);
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
  for (byte y = 0; y <= virtscreenysize - 1; y++) {
    for (byte x = 0; x <= virtscreenxsize - 1; x++) {
      virtscreen[x][y] = " ";
    }
  }
#endif
  copyvirtscreen2lcd(0, 0);
}

// main function
void loop() {
  String s;
  // detect LCD backlight off time
  currenttime = millis();
  if (currenttime - previoustime >= bloffinterval)
  {
    lcd_backlight(0);
  } else
  {
    lcd_backlight(1);
  }
  // get operation mode
  if (getmode() != operationmode) {
    operationmode = getmode();
    s = msg[8] + String(operationmode);
    Serial.println(" * " + s);
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print(s);
    delay(1000);
    lcd.clear();
  }
  // handling serial ports
  com_handler(1);
  // com_handler(2);
  // handling buttons
  btn_handler(operationmode);
}
