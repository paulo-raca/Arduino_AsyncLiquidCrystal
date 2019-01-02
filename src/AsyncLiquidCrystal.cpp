#include "AsyncLiquidCrystal.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

class Interrupts {
    uint8_t sreg;
public:
    Interrupts(bool enabled=false) {
        sreg = SREG;
        if (enabled) {
            interrupts();
        } else {
            noInterrupts();
        }
    }
    ~Interrupts() {
        SREG = sreg;
    }
};

#define WITHOUT_INTERRUPTION(CODE) {Interrupts _interrupts_handle(false); {CODE}}
#define WITH_INTERRUPTION(CODE) {Interrupts _interrupts_handle(true); {CODE}}


#define LCD_QUEUE_INIT_DELAY     0x00
#define LCD_QUEUE_INIT_0x30_SLOW 0x01
#define LCD_QUEUE_INIT_0x30      0x02
#define LCD_QUEUE_INIT_0x20      0x03
#define LCD_QUEUE_CMD            0x04
#define LCD_QUEUE_WRITE          0x05


#define LCD_STATE_READY            0x00
#define LCD_STATE_WAIT_EXECUTION   0x01

#define LCD_TIME(micros) (micros + micros/8 + 1)  // Add ~12.5% slack over official timmings to compensate for different LCDs

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
			     uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
  init(1, rs, rw, enable, 0, 0, 0, 0, d4, d5, d6, d7);
}

AsyncLiquidCrystal::AsyncLiquidCrystal(uint8_t rs,  uint8_t enable,
			     uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
  init(1, rs, 255, enable, 0, 0, 0, 0, d4, d5, d6, d7);
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
  digitalWrite(_rs_pin, LOW);
  
  // we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
  if (_rw_pin != 255) { 
    pinMode(_rw_pin, OUTPUT);
    digitalWrite(_rw_pin, LOW);
  }
  pinMode(_enable_pin, OUTPUT);
  digitalWrite(_enable_pin, LOW);
  
  // Do these once, instead of every time a character is drawn for speed reasons.
  for (int i=((_displayfunction & LCD_8BITMODE) ? 0 : 4); i<8; ++i) {
    pinMode(_data_pins[i], OUTPUT);
    digitalWrite(_data_pins[i], LOW);
  }
  
  //Enqueue reset commands
  WITHOUT_INTERRUPTION({
    state = LCD_STATE_READY;
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
  
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 18) {
      return false;
    } else {
      queue.write(LCD_QUEUE_CMD);
      queue.write(LCD_SETCGRAMADDR | (location << 3));
        
      for (int i=0; i<8; i++) {
        queue.write(LCD_QUEUE_WRITE);
        queue.write(charmap[i]);
      }
      return true;
    }
  })
}

/*********** mid level commands, for sending data/cmds */

inline bool AsyncLiquidCrystal::command(uint8_t value) {
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 2) {
      return false;
    } else {
      queue.write(LCD_QUEUE_CMD);
      queue.write(value);
      return true;
    }
  })
}

inline size_t AsyncLiquidCrystal::write(uint8_t value) {
  WITHOUT_INTERRUPTION({
    if (queue.availableForWrite() < 2) {
      return 0;
    } else {
      queue.write(LCD_QUEUE_WRITE);
      queue.write(value);
      return 1;
    }
  })
}

long AsyncLiquidCrystal::processQueue() { 
  uint8_t cmd;
  uint8_t cmd_data;
  long now = micros();
  WITHOUT_INTERRUPTION({
    if (state != LCD_STATE_READY) {
      long wait = wait_until - now;
      if (wait > 0) {
        return wait;
      } else {
        state = LCD_STATE_READY;
      }
    }
    
    if (!queue.available()) {
      return -1;
    }
    
    //TODO: Maybe add more intermediate states to avoid `delay_micros(1)` when strobbing EN
    //OTOH, 1us is just a handful of cycles on an AVR, so yielding may not be a good idea at all.
    cmd = queue.read();
    if ((cmd == LCD_QUEUE_CMD) || (cmd == LCD_QUEUE_WRITE)) {
      cmd_data = queue.read();
    }
  })
    
  long delay = 0;
  switch (cmd) {
    case LCD_QUEUE_INIT_DELAY: {
      delay = 4000;
      state = LCD_STATE_WAIT_EXECUTION;
      break;
    }
    case LCD_QUEUE_INIT_0x30_SLOW: 
    case LCD_QUEUE_INIT_0x30 : 
    case LCD_QUEUE_INIT_0x20: {
      digitalWrite(_rs_pin, LOW);
      
      digitalWrite(_data_pins[7], LOW);
      digitalWrite(_data_pins[6], LOW);
      digitalWrite(_data_pins[5], HIGH);
      digitalWrite(_data_pins[4], (cmd == LCD_QUEUE_INIT_0x20) ? LOW : HIGH);
      
      delayMicroseconds(1);
      digitalWrite(_enable_pin, HIGH);
      delayMicroseconds(1);
      digitalWrite(_enable_pin, LOW);
      
      delay = (cmd == LCD_QUEUE_INIT_0x30_SLOW ? 4100 : 100);
      state = LCD_STATE_WAIT_EXECUTION;
      break;
    }
    case LCD_QUEUE_CMD:
    case LCD_QUEUE_WRITE: {
      digitalWrite(_rs_pin, cmd == LCD_QUEUE_CMD ? LOW : HIGH);
      if (_displayfunction & LCD_8BITMODE) {
        digitalWrite(_data_pins[0], cmd_data & (1<<0));
        digitalWrite(_data_pins[1], cmd_data & (1<<1));
        digitalWrite(_data_pins[2], cmd_data & (1<<2));
        digitalWrite(_data_pins[3], cmd_data & (1<<3));
        digitalWrite(_data_pins[4], cmd_data & (1<<4));
        digitalWrite(_data_pins[5], cmd_data & (1<<5));
        digitalWrite(_data_pins[6], cmd_data & (1<<6));
        digitalWrite(_data_pins[7], cmd_data & (1<<7));
        
        delayMicroseconds(1);
        digitalWrite(_enable_pin, HIGH);
        delayMicroseconds(1);
        digitalWrite(_enable_pin, LOW);
      } else {
        digitalWrite(_data_pins[4], cmd_data & (1<<4));
        digitalWrite(_data_pins[5], cmd_data & (1<<5));
        digitalWrite(_data_pins[6], cmd_data & (1<<6));
        digitalWrite(_data_pins[7], cmd_data & (1<<7));
        
        delayMicroseconds(1);
        digitalWrite(_enable_pin, HIGH);
        delayMicroseconds(1);
        digitalWrite(_enable_pin, LOW);
        
        digitalWrite(_data_pins[4], cmd_data & (1<<0));
        digitalWrite(_data_pins[5], cmd_data & (1<<1));
        digitalWrite(_data_pins[6], cmd_data & (1<<2));
        digitalWrite(_data_pins[7], cmd_data & (1<<3));
        
        delayMicroseconds(1);
        digitalWrite(_enable_pin, HIGH);
        delayMicroseconds(1);
        digitalWrite(_enable_pin, LOW);
      }
      
      if (cmd == LCD_QUEUE_WRITE) {
        delay = 41;
      } else {
        if (cmd_data & (LCD_SETDDRAMADDR|LCD_SETCGRAMADDR|LCD_FUNCTIONSET|LCD_CURSORSHIFT|LCD_DISPLAYCONTROL|LCD_ENTRYMODESET)) {
          delay = 37;
        } else if (cmd_data & (LCD_RETURNHOME|LCD_CLEARDISPLAY)) {
          delay = 1520;
        } else {  // No-op
          delay = 37;
        }
      }
      state = LCD_STATE_WAIT_EXECUTION;
    }
  }
  
  delay = LCD_TIME(delay);
  wait_until = micros() + delay;
  
  return delay;
}

void AsyncLiquidCrystal::flush() { 
  while (true) {
    if (processQueue() < 0) {
      return;
    }
  }
}

