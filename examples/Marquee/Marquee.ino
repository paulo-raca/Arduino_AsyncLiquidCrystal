 /*
 * This examples has 2 tasks, one producing data to display on the LCD, another one doing the actual communication with it.
 * Everything is orchestrated by DeepSleepScheduler.
*/

// include the library code:
#include <AsyncLiquidCrystal.h>
#include <DeepSleepScheduler.h>

#define ROW_LEN 20

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
AsyncLiquidCrystal lcd(PIN_PH7, PIN_PG3, PIN_PG4,
                  PIN_PL0, PIN_PL1, PIN_PL2, PIN_PL6);

void scheduledDisplayRefresh() {
    long delay = lcd.processQueue();
    if (delay >= 0) {
        scheduler.scheduleDelayed(scheduledDisplayRefresh, delay/1000);
    }
}

void rescheduledDisplayRefresh() {
    scheduler.removeCallbacks(scheduledDisplayRefresh);
    scheduler.schedule(scheduledDisplayRefresh);
}

void scheduledDisplayNextChar() {
    static char c = 0;
    static char row_first = 'A';
    static uint8_t row_count = ROW_LEN;

    bool sched = false;
    while (true) {
        if (row_count == ROW_LEN) {
            if (lcd.setCursor(0, 0)) {
                row_count = 0;
                c = row_first;
                row_first++;
                if (row_first > 'Z') {
                    row_first = 'A';
                }
                scheduler.scheduleDelayed(scheduledDisplayNextChar, 100);
                break;
            } else {
              scheduler.schedule(scheduledDisplayNextChar);
              break;
            }
        } else {
            if (lcd.print(c)) {
                sched = true;
                c++;
                row_count++;
                if (c > 'Z') {
                    c = 'A';
                }
            } else {
              scheduler.schedule(scheduledDisplayNextChar);
              break;
            }
        }
    }

    if (sched) {
        rescheduledDisplayRefresh();
    }
}

void setup() { 
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  scheduler.schedule(scheduledDisplayRefresh);
  scheduler.schedule(scheduledDisplayNextChar);
}

void loop() {
  scheduler.execute();
}
