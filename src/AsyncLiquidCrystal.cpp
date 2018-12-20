#include "AsyncLiquidCrystal.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

#define WITHOUT_INTERRUPTION(CODE) {uint8_t sreg = SREG; noInterrupts(); {CODE} SREG = sreg;}
#define WITH_INTERRUPTION(CODE) {uint8_t sreg = SREG; interrupts(); {CODE} SREG = sreg;}


#define LCD_QUEUE_INIT_DELAY     0x00
#define LCD_QUEUE_INIT_0x30_SLOW 0x01
#define LCD_QUEUE_INIT_0x30      0x02
#define LCD_QUEUE_INIT_0x20      0x03
#define LCD_QUEUE_CMD            0x04
#define LCD_QUEUE_WRITE          0x05

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set: 
//    DL = 1; 8-bit interface data 
//    N = 0; 1-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 0; Display off 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1 
//    S = 0; No shift 
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// AsyncLiquidCrystal constructor is called).

AsyncLiquidCrystal::AsyncLiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
			     uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
			     uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
  init(0, rs, rw, enable, d0, d1, d2, d3, d4, d5, d6, d7);
}

AsyncLiquidCrystal::AsyncLiquidCrystal(uint8_t rs, uint8_t enable,
			     uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
			     uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
  init(0, rs, 255, enable, d0, d1, d2, d3, d4, d5, d6, d7);
}

AsyncLiquidCrystal::AsyncLiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
			     uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
  init(1, rs, rw, enable, d0, d1, d2, d3, 0, 0, 0, 0);
}

AsyncLiquidCrystal::AsyncLiquidCrystal(uint8_t rs,  uint8_t enable,
			     uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
  init(1, rs, 255, enable, d0, d1, d2, d3, 0, 0, 0, 0);
}

void AsyncLiquidCrystal::init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
			 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
			 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
  _rs_pin = rs;
  _rw_pin = rw;
  _enable_pin = enable;
  
  _data_pins[0] = d0;
  _data_pins[1] = d1;
  _data_pins[2] = d2;
  _data_pins[3] = d3; 
  _data_pins[4] = d4;
  _data_pins[5] = d5;
  _data_pins[6] = d6;
  _data_pins[7] = d7; 

  if (fourbitmode)
    _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
  else 
    _displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
  
  begin(16, 1);  
}

void AsyncLiquidCrystal::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;

  setRowOffsets(0x00, 0x40, 0x00 + cols, 0x40 + cols);  

  // for some 1 line displays you can select a 10 pixel high font
  if ((dotsize != LCD_5x8DOTS) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  }

  pinMode(_rs_pin, OUTPUT);
  // we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
  if (_rw_pin != 255) { 
    pinMode(_rw_pin, OUTPUT);
    digitalWrite(_rw_pin, LOW);
  }
  pinMode(_enable_pin, OUTPUT);
  digitalWrite(_enable_pin, LOW);
  
  // Do these once, instead of every time a character is drawn for speed reasons.
  for (int i=0; i<((_displayfunction & LCD_8BITMODE) ? 8 : 4); ++i)
  {
    pinMode(_data_pins[i], OUTPUT);
   } 
  
  //Enqueue reset commands
  WITHOUT_INTERRUPTION({
    queue.clear();
    queue.write(LCD_QUEUE_INIT_DELAY);
    queue.write(LCD_QUEUE_INIT_0x30_SLOW);
    queue.write(LCD_QUEUE_INIT_0x30);
    queue.write(LCD_QUEUE_INIT_0x30);

    if (! (_displayfunction & LCD_8BITMODE)) {
      queue.write(LCD_QUEUE_INIT_0x20);
    }
    

    // finally, set # lines, font size, etc.
    queue.write(LCD_QUEUE_CMD);
    queue.write(LCD_FUNCTIONSET | _displayfunction);
    
    // turn the display on with no cursor or blinking default
    _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;  
    queue.write(LCD_QUEUE_CMD);
    queue.write(LCD_DISPLAYCONTROL | _displaycontrol);
  
    // clear it off
    queue.write(LCD_QUEUE_CMD);
    queue.write(LCD_CLEARDISPLAY);
    
    // Initialize to default text direction (for romance languages)
    _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    // set the entry mode
    queue.write(LCD_QUEUE_CMD);
    queue.write(LCD_ENTRYMODESET | _displaymode);
  })
}

void AsyncLiquidCrystal::setRowOffsets(int row0, int row1, int row2, int row3)
{
  _row_offsets[0] = row0;
  _row_offsets[1] = row1;
  _row_offsets[2] = row2;
  _row_offsets[3] = row3;
}

/********** high level commands, for the user! */
bool AsyncLiquidCrystal::clear()
{
  return command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
}

bool AsyncLiquidCrystal::home()
{
  return command(LCD_RETURNHOME);  // set cursor position to zero
}

bool AsyncLiquidCrystal::setCursor(uint8_t col, uint8_t row)
{
  const size_t max_lines = sizeof(_row_offsets) / sizeof(*_row_offsets);
  if ( row >= max_lines ) {
    row = max_lines - 1;    // we count rows starting w/0
  }
  if ( row >= _numlines ) {
    row = _numlines - 1;    // we count rows starting w/0
  }
  
  return command(LCD_SETDDRAMADDR | (col + _row_offsets[row]));
}

// Turn the display on/off (quickly)
bool AsyncLiquidCrystal::noDisplay() {
  uint8_t newdisplaycontrol = _displaycontrol & ~LCD_DISPLAYON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}
bool AsyncLiquidCrystal::display() {
  uint8_t newdisplaycontrol = _displaycontrol | LCD_DISPLAYON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}

// Turns the underline cursor on/off
bool AsyncLiquidCrystal::noCursor() {
  uint8_t newdisplaycontrol = _displaycontrol & ~LCD_CURSORON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}
bool AsyncLiquidCrystal::cursor() {
  uint8_t newdisplaycontrol = _displaycontrol | LCD_CURSORON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}

// Turn on and off the blinking cursor
bool AsyncLiquidCrystal::noBlink() {
  uint8_t newdisplaycontrol = _displaycontrol & ~LCD_BLINKON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}
bool AsyncLiquidCrystal::blink() {
  uint8_t newdisplaycontrol = _displaycontrol | LCD_BLINKON;
  bool ret = command(LCD_DISPLAYCONTROL | newdisplaycontrol);
  if (ret) {
      _displaycontrol = newdisplaycontrol;
  }
  return ret;
}

// These commands scroll the display without changing the RAM
bool AsyncLiquidCrystal::scrollDisplayLeft(void) {
  return command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
bool AsyncLiquidCrystal::scrollDisplayRight(void) {
  return command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
bool AsyncLiquidCrystal::leftToRight(void) {
  uint8_t newdisplaymode = _displaymode | LCD_ENTRYLEFT;
  bool ret = command(LCD_ENTRYMODESET | newdisplaymode);
  if (ret) {
      _displaymode = newdisplaymode;
  }
  return ret;
}

// This is for text that flows Right to Left
bool AsyncLiquidCrystal::rightToLeft(void) {
  uint8_t newdisplaymode = _displaymode & ~LCD_ENTRYLEFT;
  bool ret = command(LCD_ENTRYMODESET | newdisplaymode);
  if (ret) {
      _displaymode = newdisplaymode;
  }
  return ret;
}

// This will 'right justify' text from the cursor
bool AsyncLiquidCrystal::autoscroll(void) {
  uint8_t newdisplaymode = _displaymode | LCD_ENTRYSHIFTINCREMENT;
  bool ret = command(LCD_ENTRYMODESET | newdisplaymode);
  if (ret) {
      _displaymode = newdisplaymode;
  }
  return ret;
}

// This will 'left justify' text from the cursor
bool AsyncLiquidCrystal::noAutoscroll(void) {
  uint8_t newdisplaymode = _displaymode & ~LCD_ENTRYSHIFTINCREMENT;
  bool ret = command(LCD_ENTRYMODESET | newdisplaymode);
  if (ret) {
      _displaymode = newdisplaymode;
  }
  return ret;
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
bool AsyncLiquidCrystal::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  
  bool ret;
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 18) {
      ret = false;
    } else {
      queue.write(LCD_QUEUE_CMD);
      queue.write(LCD_SETCGRAMADDR | (location << 3));
        
      for (int i=0; i<8; i++) {
        queue.write(LCD_QUEUE_WRITE);
        queue.write(charmap[i]);
      }
      ret = true;
    }
  })
  return ret;
}

/*********** mid level commands, for sending data/cmds */

inline bool AsyncLiquidCrystal::command(uint8_t value) {
  bool ret;
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 2) {
      ret = false;
    } else {
      queue.write(LCD_QUEUE_CMD);
      queue.write(value);
      ret = true;
    }
  })
  return ret;
}

inline size_t AsyncLiquidCrystal::write(uint8_t value) {
  size_t ret;
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 2) {
      ret = 0;
    } else {
      queue.write(LCD_QUEUE_WRITE);
      queue.write(value);
      ret = 1;
    }
  })
  return ret;
}

/************ low level data pushing commands **********/

// write either command or data, with automatic 4/8-bit selection
bool AsyncLiquidCrystal::send(uint8_t value, uint8_t mode) {
  digitalWrite(_rs_pin, mode);

  // if there is a RW pin indicated, set it low to Write
  if (_rw_pin != 255) { 
    digitalWrite(_rw_pin, LOW);
  }
  
  if (_displayfunction & LCD_8BITMODE) {
    write8bits(value); 
  } else {
    write4bits(value>>4);
    write4bits(value);
  }
}

void AsyncLiquidCrystal::pulseEnable(void) {
  digitalWrite(_enable_pin, LOW);
  delayMicroseconds(1);    
  digitalWrite(_enable_pin, HIGH);
  delayMicroseconds(1);    // enable pulse must be >450ns
  digitalWrite(_enable_pin, LOW);
  delayMicroseconds(100);   // commands need > 37us to settle
}

void AsyncLiquidCrystal::write4bits(uint8_t value) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(_data_pins[i], (value >> i) & 0x01);
  }

  pulseEnable();
}

void AsyncLiquidCrystal::write8bits(uint8_t value) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(_data_pins[i], (value >> i) & 0x01);
  }
  
  pulseEnable();
}
