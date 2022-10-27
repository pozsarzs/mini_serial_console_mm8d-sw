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
            20x4 size displayed area on 80x4 size virtual screen,
            automatically scrolling lines,
            the displayed area can be moved horizontally with pushbuttons.
   Mode #1: read only mode,
            no cursor,
            20x4 size displayed area on 80x25 size virtual screen,
            automatically scrolling lines,
            the displayed area can be moved horizontally and vertically with pushbuttons.
   Mode #2: read only mode,
            no cursor,
            20x4 size displayed area on 80x25 size virtual screen,
            the displayed area can be moved horizontally and vertically with pushbuttons,
            after 2000 characters or FormFeed (0x12), a new, clean page starts.
   Mode #3: reserved for device depend solutions (menu, read/write mode etc.)


   Button functions:

    Button  Mode #0    Mode #1    Mode #2    Mode #3
    ---------------------------------------------------
    PB0     move left  move left  move left  move left
    PB1     move right move right move right move right
    PB2                move up    move up    move up
    PB3                move down  move down  move down
    PB4                                      enter
    PB5                                      escape


   Hitachi HD44780 compatible 20x4 size LCD:

   Registers: 8 bit instruction register (IR), access: RS=0
              8 bit data register (DR), access: RS=1

   Display Data RAM (DDRAM):   address of 1st line: 0x00-0x13
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

    Instruction                RS  R/W DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
    ------------------------------------------------------------------
    Clear display              0   0   0   0   0   0   0   0   0   1
    Return home                0   0   0   0   0   0   0   0   1   x
    Entry mode set             0   0   0   0   0   0   0   1   I/D S
    Display on/off control     0   0   0   0   0   0   1   D   C   B
    Cursor or display shift    0   0   0   0   0   1   S/C R/L x   x
    Function set               0   0   0   0   1   DL  N   F   x   x
    Set CGRAM address          0   0   0   1   ACG ACG ACG ACG ACG ACG
    Set DDRAM address          0   0   1   ADD ADD ADD ADD ADD ADD ADD
    Write data to CG or DDRAM  1   0
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
      : Not used because the R/-W input is shorted to GND.

*/

# define TEST

// settings
const int     spd_serial0       = 115200;       // speed of the USB serial port
const int     spd_serial1       = 38400;        // speed of the TTL serial port
const int     spd_serial2       = 38400;        // speed of the RS232C serial port
// GPIO ports
const int     lcd_bl            = 14;           // LCD - backlight on/off
const int     lcd_db0           = 2;            // LCD - databit 0
const int     lcd_db1           = 3;            // LCD - databit 1
const int     lcd_db2           = 4;            // LCD - databit 2
const int     lcd_db3           = 5;            // LCD - databit 3
const int     lcd_db4           = 10;           // LCD - databit 4
const int     lcd_db5           = 11;           // LCD - databit 5
const int     lcd_db6           = 12;           // LCD - databit 6
const int     lcd_db7           = 13;           // LCD - databit 7
const int     lcd_en            = 7;            // LCD - enable
const int     lcd_rs            = 6;            // LCD - register select
const int     prt_jp2           = 16;           // operation mode (JP2 jumper)
const int     prt_jp3           = 15;           // operation mode (JP3 jumper)
const int     prt_pb0           = 17;           // pushbutton 0
const int     prt_pb1           = 18;           // pushbutton 1
const int     prt_pb2           = 19;           // pushbutton 2
const int     prt_pb3           = 20;           // pushbutton 3
const int     prt_pb4           = 21;           // pushbutton 4
const int     prt_pb5           = 22;           // pushbutton 5
const int     prt_rxd2          = 9;            // RXD line of the serial port 2
const int     prt_txd2          = 8;            // TXD line of the serial port 2
const int     prt_led           = LED_BUILTIN;
// general constants
const String  swversion         = "0.1";        // version of this program
// general variables
int puffer[80][25];                             // virtual display
// messages
String msg[40]                  =
{
  /*  0 */  "",
  /*  1 */  "Mini serial console",
  /*  2 */  "Software: v",
  /*  3 */  "(C)2022 Pozsar Zsolt",
  /*  4 */  "Initializing...",
  /*  5 */  " * Set speed of serial ports #1-#2: ",
  /*  6 */  " * Set GPIO ports",
  /*  7 */  " * Set operation mode of LCD",
  /*  8 */  " * Operation mode of mini console device is #",
  /*  9 */  " * WARNING! Mini console is in the test mode.",
  /* 10 */  "Read data from serial port #",
  /* 11 */  "Button pressing detected: PB",
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

UART Serial2(prt_txd2, prt_rxd2, 0, 0);

// initializing
void setup() {
  // set USB serial port
  Serial.begin(spd_serial0);
  // write program information
  Serial.println("");
  Serial.println("");
  Serial.println(msg[1]);
  Serial.println(msg[2] + swversion);
  Serial.println(msg[3]);
  Serial.println(msg[4]);
  // set other serial ports
  Serial.println(msg[5] + String((int)spd_serial1) + "b/s - " + String((int)spd_serial2) + "b/s");
  Serial2.begin(spd_serial1);
  Serial2.begin(spd_serial2);
  // set GPIO ports
  Serial.println(msg[6]);
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
  // Set LCD
  Serial.println(msg[7]);
  lcd_init();
  lcd_clear();
  lcd_functionset(1, 1, 0); // function set: 8-bit mode, 2-line display and 5 × 8 dot character font
  lcd_writecommand(0x0C);   // display on/off control: display on, cursor and blinking off
  delay(10);
  lcd_writecommand(0x06);   // entry mode set: increment cursor, no shift
  delay(10);
#ifdef TEST
  // data for test mode (without serial input)
  Serial.println(msg[9]);
  for (int r = 1; r < 26; r++) {
    for (int c = 1; c < 81; c++) {
      if (c == 1) {
        if (r < 10) {
          puffer[c - 1][r - 1] = 48;
        }
        if (r > 9 and r < 20) {
          puffer[c - 1][r - 1] = 49;
        }
        if (r > 19) {
          puffer[c - 1][r - 1] = 50;
        }
      }
      if (c == 2) {
        if (r < 10) {
          puffer[c - 1][r - 1] = int(r);
        }
        if (r > 9 and c < 20) {
          puffer[c - 1][r - 1] = int(r - 10);
        }
        if (r > 19) {
          puffer[c - 1][r - 1] = int(r - 20);
        }
      }
      if (c == 3) {
        puffer[c - 1][r - 1] = 95;
      }
      if (c >= 4) {
        if (c >= 4 and c <= 29) {
          puffer[c - 1][r - 1] = c + 93;
        }
        if (c == 30 or c == 57) {
          puffer[c - 1][r - 1] = 32;
        }
        if (c >= 31 and c <= 56) {
          puffer[c - 1][r - 1] = c + 34;
        }
        if (c >= 58) {
          puffer[c - 1][r - 1] = c + 39;
        }
      }
    }
  }
#endif
}

// main function
void loop() {
  int mode = 0;
  // get operation mode
  mode = digitalRead(prt_jp3) * 2 + digitalRead(prt_jp3);
  Serial.println(msg[8] + String((int)mode));
#ifndef TEST
  // handling serial ports
  com_handler(1);
  com_handler(2);
#endif
  // handling buttons and display
  btn_handler(mode);
}

// read communication port and move data to puffer
void com_handler(int p) {
  switch (p) {
    case 1:
      if (Serial1.available()) {
        Serial.println(msg[10] + '1');
        digitalWrite(prt_led, HIGH);
        // Serial1.read();
      }
      break;
    case 2:
      if (Serial2.available()) {
        Serial.println(msg[10] + '2');
        digitalWrite(prt_led, HIGH);
        // Serial2.read();
      }
      break;
    default:
      digitalWrite(prt_led, LOW);
      break;
  }
}

// read status of pushbuttons and write text to LCD
void btn_handler(int m) {
  int x = 0;
  int y = 0;
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
  lcd_writelines(x, y);
}

// write text to display
void lcd_writelines(int x, int y) {

}

// -- Base display functions --

// LCD - set DDRAM address
void lcd_functionset(int addr) {
  int cmd = 0x80;
  lcd_writecommand(cmd or addr);
}

// LCD - function set
void lcd_functionset(int dl, int n, int f) {
  int cmd = 0x20;
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
void lcd_displayonoffcontrol(int sc, int rl) {
  int cmd = 0x10;
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
void lcd_displayonoffcontrol(int d, int c, int b) {
  int cmd = 0x08;
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
void lcd_entrymode(int i, int s) {
  int cmd = 0x04;
  if (i) {
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

// LCD - write a character
void lcd_writecharacter(char cmd) {
  digitalWrite(lcd_rs, HIGH);
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

// LCD - write a command
void lcd_writecommand(char cmd) {
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

// LCD - turn on/off background light
void lcd_backlight(int bl) {
  if (bl) {
    digitalWrite(lcd_bl, HIGH);
  } else {
    digitalWrite(lcd_bl, LOW);
  }
}
