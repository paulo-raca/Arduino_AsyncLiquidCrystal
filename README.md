# Async Liquid Crystal Library for Arduino

This library is a derivative of Arduino's [LiquidCrystal](http://www.arduino.cc/en/Reference/LiquidCrystal) library, extended to support asynchronous operations.


## Usage

The usage is basically the same as the original library, please see [LiquidCrystal reference](http://www.arduino.cc/en/Reference/LiquidCrystal).

Differences are:
- Operations are queued internally, therefore funcions return immediately.
- if the internal queue is full, functions will return false (write() returns the number of bytes queued).
It's up to the caller code to re-executed failed operations later.
- After issuing commands, you must give it a chance to execute the queued commands with `processQueue()` or `flush()`.

### `processQueue()`

This method will execute pending commands from the internal queue, and return ASAP.

It will also return the amount of time, in microseconds, you should wait until the next command can be executed (that is, when you should call `processQueue()` again).

If there are no more commands to execute, it will return -1.

Calling this again before the time has ellapsed is a no-op: It will just tell you to wait a bit more.
On the other hand, waiting more than the suggested time will slow down your refresh, but is otherwise harmless.
   
### `flush()`

This will execute all commands in the internal queue, blocking for the time necessary.

It's the same as calling `processQueue()` until it returns -1.
