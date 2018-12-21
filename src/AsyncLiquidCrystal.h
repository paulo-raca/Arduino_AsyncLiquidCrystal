#ifndef AsyncLiquidCrystal_h
#define AsyncLiquidCrystal_h

#include <LoopbackStream.h>

#include <inttypes.h>
#include "Print.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

class AsyncLiquidCrystal : public Print {
public:
  AsyncLiquidCrystal(uint8_t rs, uint8_t enable,
		uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
  AsyncLiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
		uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
  AsyncLiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
		uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
  AsyncLiquidCrystal(uint8_t rs, uint8_t enable,
		uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

  void init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
	    uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
	    uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);
    
  void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS);

  bool clear();
  bool home();

  bool noDisplay();
  bool display();
  bool noBlink();
  bool blink();
  bool noCursor();
  bool cursor();
  bool scrollDisplayLeft();
  bool scrollDisplayRight();
  bool leftToRight();
  bool rightToLeft();
  bool autoscroll();
  bool noAutoscroll();

  void setRowOffsets(int row1, int row2, int row3, int row4);
  bool createChar(uint8_t, uint8_t[]);
  bool setCursor(uint8_t, uint8_t); 
  virtual size_t write(uint8_t);
  bool command(uint8_t);
  
  /**
   * This will execute pending commands from the internal queue.
   * While all the various commands in this class return immediately, 
   * you should processQueue flush() ASAP after calling then.
   * 
   * If processQueue() returns -1, the queue is empty and it doesn't need to be called again until new commands are issued.
   * Otherwise, you should call flush() again after the specified number of microseconds to process the next element in the queue.
   */
  long processQueue();
  
  /**
   * This will block until the whole queue has been processed.
   * 
   * It's the same as calling processQueue() until it returns -1
   */
  virtual void flush();
  
  using Print::write;
private:
  unsigned long wait_until;
  uint8_t state;
  LoopbackStream queue;

  uint8_t _rs_pin; // LOW: command.  HIGH: character.
  uint8_t _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
  uint8_t _enable_pin; // activated by a HIGH pulse.
  uint8_t _data_pins[8];

  uint8_t _displayfunction;
  uint8_t _displaycontrol;
  uint8_t _displaymode;

  uint8_t _initialized;

  uint8_t _numlines;
  uint8_t _row_offsets[4];
};

#endif
