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


   Hitachi HD44780 compatible 20x4 size LCD:

    Registers: 8 bit instruction register (IR), access: RS=0
               8 bit data register (DR), access: RS=1

    Display Data RAM (DDRAM): address of 1st line: 0x00-0x13
                              address of 2nd line: 0x40-0x53
                              address of 3rd line: 0x14-0x27
                              address of 4th line: 0x54-0x67

    Standard wake up procedure: - power on
                                - wait for min. 15 ms
                                - write 0x30 to IR
                                - wait for min. 4.1 ms
                                - write 0x30 to IR
                                - wait for min. 0.1 ms
                                - write 0x30 to IR

    Instructions codes:

    Instruction                 RS  R/W DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
    -------------------------------------------------------------------
    Clear display               0   0   0   0   0   0   0   0   0   1
    Return home                 0   0   0   0   0   0   0   0   1   x
    Entry mode set              0   0   0   0   0   0   0   1   I/D S
    Display on/off control      0   0   0   0   0   0   1   D   C   B
    Cursor or display shift     0   0   0   0   0   1   S/C R/L x   x
    Function set                0   0   0   0   1   DL  N   F   x   x
    Set CGRAM address           0   0   0   1   ACG ACG ACG ACG ACG ACG
    Set DDRAM address           0   0   1   ADD ADD ADD ADD ADD ADD ADD
    Write data to CG or DDRAM   1   0
    (Read busy flag, address    0   1   BF  AC  AC  AC  AC  AC  AC  AC)*
    (Read data from CG or DDRAM 1   1                                 )*

    Notes:
     B = 0: Blinking off
     B = 1: Blinking on
     BF = 0:  Instructions acceptable
     BF = 1: Internally operating
     C = 0: Cursor off
     C = 1: Cursor on
     D = 0: Display off
     D = 1: Display on
     DL = 0: 4 bits
     DL = 1: 8 bits
     F = 0: 5 × 8 dots font size
     F = 1: 5 × 10 dots font size
     I/D = 0: Decrement address counter
     I/D = 1: Increment address counter
     N = 0: 1 line
     N = 1: 2 lines
     R/L = 0: Shift to the left
     R/L = 1: Shift to the right
     S = 1: Accompanies display shift
     S/C = 0: Cursor move
     S/C = 1:  Display shift
     DDRAM: Display data RAM
     CGRAM: Character generator RAM
     ACG: CGRAM address
     ADD: DDRAM address (corresponds to cursor address)
     AC: Address counter used for both DD and CGRAM addresses
     '*' : Not used because the R/-W input is shorted to GND.
*/

# define TEST

// settings
const int     spd_serial[3]     = {38400, 9600, 9600}; // speed of the USB serial port
const byte    virtscreenxsize   = 80;                  // horizontal size of virtual screen
const byte    virtscreenysize   = 25;                  // vertical size of virtual screen
// GPIO ports
const byte    lcd_bl            = 14;                  // LCD - backlight on/off
const byte    lcd_db0           = 2;                   // LCD - databit 0
const byte    lcd_db1           = 3;                   // LCD - databit 1
const byte    lcd_db2           = 4;                   // LCD - databit 2
const byte    lcd_db3           = 5;                   // LCD - databit 3
const byte    lcd_db4           = 10;                  // LCD - databit 4
const byte    lcd_db5           = 11;                  // LCD - databit 5
const byte    lcd_db6           = 12;                  // LCD - databit 6
const byte    lcd_db7           = 13;                  // LCD - databit 7
const byte    lcd_en            = 7;                   // LCD - enable
const byte    lcd_rs            = 6;                   // LCD - register select
const byte    prt_jp2           = 16;                  // operation mode (JP2 jumper)
const byte    prt_jp3           = 15;                  // operation mode (JP3 jumper)
const byte    prt_pb0           = 17;                  // pushbutton 0
const byte    prt_pb1           = 18;                  // pushbutton 1
const byte    prt_pb2           = 19;                  // pushbutton 2
const byte    prt_pb3           = 20;                  // pushbutton 3
const byte    prt_pb4           = 21;                  // pushbutton 4
const byte    prt_pb5           = 22;                  // pushbutton 5
const byte    prt_rxd2          = 9;                   // RXD line of the serial port 2
const byte    prt_txd2          = 8;                   // TXD line of the serial port 2
const byte    prt_led           = LED_BUILTIN;         // LED on the board of Pico
// general constants
const String  swversion         = "0.1";               // version of this program
// general variables
byte virtscreen[virtscreenxsize][virtscreenysize];     // virtual screen
byte virtscreenypos             = 0;                   // y pos. for copy data (rxdbuffer->virtscreen)
byte mode                       = 0;                   // operation mode of device
// messages
String msg[20]                  =
{
  /*  0 */  "",
  /*  1 */  "Mini serial console",
  /*  2 */  "Software: v",
  /*  3 */  "(C)2022 Pozsar Zsolt",
  /*  4 */  "Initializing...",
  /*  5 */  " * GPIO ports",
  /*  6 */  " * LCD",
  /*  7 */  " * Speed of ports:",
  /*  8 */  " * Operation mode: #",
  /*  9 */  "Read a line from serial port #",
  /* 10 */  " * I read ",
  /* 11 */  " * Line too long, cut to ",
  /* 12 */  " * Copy line to this y position of the virtual screen: ",
  /* 13 */  "",
  /* 14 */  "",
  /* 15 */  "",
  /* 16 */  "",
  /* 17 */  "",
  /* 18 */  "",
  /* 19 */  ""
};

UART Serial2(prt_txd2, prt_rxd2, 0, 0);

// * * * BASE DISPLAY FUNCTIONS * * *

// LCD - turn on/off background light
void lcd_backlight(byte bl) {
  if (bl) {
    digitalWrite(lcd_bl, HIGH);
  } else {
    digitalWrite(lcd_bl, LOW);
  }
}

// LCD - write a character
void lcd_writecharacter(char chr) {
  digitalWrite(lcd_rs, HIGH);
  digitalWrite(lcd_db0, chr & 0x01);
  digitalWrite(lcd_db1, chr & 0x02);
  digitalWrite(lcd_db2, chr & 0x04);
  digitalWrite(lcd_db3, chr & 0x08);
  digitalWrite(lcd_db4, chr & 0x10);
  digitalWrite(lcd_db5, chr & 0x20);
  digitalWrite(lcd_db6, chr & 0x40);
  digitalWrite(lcd_db7, chr & 0x80);
  digitalWrite(lcd_en, HIGH);
  digitalWrite(lcd_en, LOW);
  delay(1);
}

// LCD - write a command
void lcd_writecommand(byte cmd) {
  digitalWrite(lcd_rs, LOW);
  digitalWrite(lcd_db0, cmd & 0x01);
  digitalWrite(lcd_db1, cmd & 0x02);
  digitalWrite(lcd_db2, cmd & 0x04);
  digitalWrite(lcd_db3, cmd & 0x08);
  digitalWrite(lcd_db4, cmd & 0x10);
  digitalWrite(lcd_db5, cmd & 0x20);
  digitalWrite(lcd_db6, cmd & 0x40);
  digitalWrite(lcd_db7, cmd & 0x80);
  digitalWrite(lcd_en, HIGH);
  digitalWrite(lcd_en, LOW);
  delay(1);
}

// LCD - set DDRAM address
void lcd_setddramaddress(byte addr) {
  byte cmd = 0x80;
  lcd_writecommand(cmd or addr);
}

// LCD - function set
void lcd_functionset(byte dl, byte n, byte f) {
  byte cmd = 0x20;
  if (dl) {
    cmd = cmd or 0x10;
  }
  if (n) {
    cmd = cmd or 0x08;
  }
  if (f) {
    cmd = cmd or 0x04;
  }
  lcd_writecommand(cmd);
}

// LCD - cursor or display shift
void lcd_cursorordisplayshift(byte sc, byte rl) {
  byte cmd = 0x10;
  if (sc) {
    cmd = cmd or 0x08;
  }
  if (rl) {
    cmd = cmd or 0x04;
  }
  lcd_writecommand(cmd);
  delay(9);
}

// LCD - display on/off control
void lcd_displayonoffcontrol(byte d, byte c, byte b) {
  byte cmd = 0x08;
  if (d) {
    cmd = cmd or 0x04;
  }
  if (c) {
    cmd = cmd or 0x02;
  }
  if (b) {
    cmd = cmd or 0x01;
  }
  lcd_writecommand(cmd);
  delay(10);
}

// LCD - set entry mode
void lcd_entrymode(byte id, byte s) {
  byte cmd = 0x04;
  if (id) {
    cmd = cmd or 0x02;
  }
  if (s) {
    cmd = cmd or 0x01;
  }
  lcd_writecommand(cmd);
  delay(9);
}

// LCD - return home
void lcd_returnhome() {
  lcd_writecommand(0x02);
  delay(9);
}

// LCD - clear
void lcd_clear() {
  lcd_writecommand(0x01);
  delay(9);
}

// LCD - initializing
void lcd_init() {
  lcd_backlight(0);
  digitalWrite(lcd_rs, 0);
  digitalWrite(lcd_en, 0);
  lcd_writecommand(0x30);
  delay(20);
  lcd_writecommand(0x30);
  delay(20);
  lcd_writecommand(0x30);
  delay(20);
}

// * * * OTHER FUNCTIONS * * *

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

// read communication port and move data to virtscr
byte com_handler(byte p) {
  const byte rxdbuffersize = 255;
  char rxdbuffer[rxdbuffersize];
  int rxdlength = 0;
  byte lastline;
  switch (p) {
    case 1:
      if (Serial1.available()) {
        Serial.println(msg[9] + String((byte)p));
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial1.readBytes(rxdbuffer, rxdbuffersize);
        Serial.print(msg[10] + String((byte)rxdlength) + "character(s).");
      }
      break;
    case 2:
      if (Serial2.available()) {
        Serial.println(msg[9] + String((byte)p));
        digitalWrite(prt_led, HIGH);
        rxdlength = Serial2.readBytes(rxdbuffer, rxdbuffersize);
        Serial.print(msg[10] + String((byte)rxdlength) + "character(s).");
      }
      break;
    default:
      digitalWrite(prt_led, LOW);
      break;
  }
  if (rxdlength > virtscreenxsize) {
    rxdlength = virtscreenxsize;
    Serial.print(msg[11] + String((byte)virtscreenxsize) + "characters.");
  }
  if (rxdlength) {
    switch (mode) {
      case 0:
        lastline = 3;
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
        break;
      case 3:
        break;
    }
  }
  return rxdlength;
}

// read status of pushbuttons and write text to LCD
void btn_handler(byte m) {
  byte x = 0;
  byte y = 0;
  // horizontal move
  if (digitalRead(prt_pb0)) {
    Serial.println(msg[11] + "0");
    if (x > 0) {
      x--;
    }
  }
  if (digitalRead(prt_pb1)) {
    Serial.println(msg[11] + "1");
    if (x + 20 < 79) {
      x++;
    }
  }
  if (m > 0) {
    // vertical move
    if (digitalRead(prt_pb2)) {
      Serial.println(msg[11] + "2");
      if (y > 0) {
        y--;
      }
    }
    if (digitalRead(prt_pb3)) {
      Serial.println(msg[11] + "3");
      if (y + 4 < 24) {
        y++;
      }
    }
  }
  if (m > 2) {
    // data input
    if (digitalRead(prt_pb4)) {
      Serial.println(msg[11] + "4");
    }
    if (digitalRead(prt_pb5)) {
      Serial.println(msg[11] + "5");
    }
  }
  display_copyfrombuffer(x, y);
}

// copy text from buffer to display
void display_copyfrombuffer(byte x, byte y) {
  display_gotoxy(0, 0);
  for (byte dy = 0; dy < 3; dy++) {
    for (byte dx = 0; dx < 19; dx++) {
      lcd_writecharacter(virtscreen[x + dx][y + dy]);
    }
  }
}

// set start position to write string
void display_gotoxy(byte dx, byte dy) {
  byte addr;
  switch (dy)
  {
    case 0: addr = 0x00; break;
    case 1: addr = 0x40; break;
    case 2: addr = 0x14; break;
    case 3: addr = 0x54; break;
  }
  addr += dx;
  lcd_setddramaddress(addr);
}

// write string to display
void display_write(String s) {
  for (byte b = 0; b < s.length(); b++)
  {
    lcd_writecharacter(s[b]);
  }
}

// get operation mode of device
byte getmode() {
  return (digitalRead(prt_jp3) * 2 + digitalRead(prt_jp3));
}

// * * * MAIN FUNCTIONS * * *

// initializing
void setup() {
  String s;
  // set USB serial port
  Serial.begin(spd_serial[0]);
  // write program information to console
  for (int b = 0; b < 3; b++) {
    if (b == 1) {
      Serial.println(msg[b + 1] + swversion);
    } else {
      Serial.println(msg[b + 1]);
    }
  }
  // set GPIO ports
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
  // clear virtual screen
  for (byte x = 0; x < 79; x++) {
    for (byte y = 0; y < 25; y++) {
      virtscreen[x][y] = 20;
    }
  }
  // set LCD
  Serial.println(msg[6]);
  lcd_init();
  lcd_clear();
  lcd_functionset(1, 1, 0);          // function set: 8-bit mode, 2-line display and 5 × 8 dot character font
  lcd_displayonoffcontrol(1, 0, 0);  // display on/off control: display on, cursor and blinking off
  lcd_entrymode(1, 0);               // entry mode set: increment cursor, no shift
  // write program information to display
  for (int b = 0; b < 3; b++) {
    display_gotoxy(0, b);
    if (b == 1) {
      display_write(msg[b + 1] + swversion);
    } else {
      display_write(msg[b + 1]);
    }
  }
  delay(3000);
  // set serial ports
  Serial1.begin(spd_serial[1]);
  Serial2.begin(spd_serial[2]);
  Serial.println(msg[4]);
  for (int b = 0; b < 3; b++) {
    s = "#" + String((byte)b) + ": " + String((int)spd_serial[b]) + " b/s";
    Serial.println("   " + s);
    display_gotoxy(0, b);
    display_write(s);
  }
  // get operation mode
  mode = digitalRead(prt_jp3) * 2 + digitalRead(prt_jp3);
  s = msg[8] + String((byte)mode);
  Serial.println(s);
  display_gotoxy(0, 3);
  display_write(s);
  delay(3000);
}

// main function
void loop() {
  String s;
  // get operation mode
  if (getmode() != mode) {
    mode = getmode();
    Serial.println(msg[8] + String((byte)mode));
    s = msg[8] + String((byte)mode);
    Serial.println(s);
    lcd_clear();
    display_gotoxy(0, 2);
    display_write(s);
    delay(3000);
    lcd_clear();
  }
  // handling serial ports
  com_handler(1);
  com_handler(2);
  // handling buttons and display
  //  display_copyfrombuffer(0, 0);
  //  btn_handler(mode);
}
