/**
 * Circuit Playground - sketch interpreter
 *
 * Runs Arduino-shaped code against the simulation clock so a block on the canvas can drive its
 * pins. This is deliberately NOT an AVR emulator: there is no instruction timing, no register
 * map and no peripheral behind any of it. What it does is the thing that matters to a circuit -
 * setup() once, loop() forever, and the pin writes in between land on nodes the solver sees.
 *
 * The one design decision everything else follows from: delay() means SIMULATED time. A sketch
 * runs against sim->time, not the wall clock, so delay(500) is half a second of the circuit's
 * own time however fast or slow the simulation is running. That is also why the code is
 * compiled to bytecode with a program counter rather than walked as a tree - a tree walker
 * cannot pause in the middle of a for loop and come back to it three thousand solver steps
 * later, and a sketch that blinks is nothing but pausing in the middle of a loop.
 */

#ifndef SKETCH_H
#define SKETCH_H

#include <stddef.h>
#include <stdbool.h>

/* Source limit. A sketch that does not fit is one that has grown past what this is for. */
#define SKETCH_MAX_SRC   8192
/* Arduino pin numbers this understands: D0..D13 and A0..A5 (which are 14..19 to the language,
   exactly as they are on the board, so a sketch saying A0 and a sketch saying 14 agree). */
#define SKETCH_MAX_PINS  20
#define SKETCH_A0        14

/* Pin direction, as pinMode leaves it. */
typedef enum {
    SKETCH_PIN_INPUT = 0,
    SKETCH_PIN_OUTPUT,
    SKETCH_PIN_INPUT_PULLUP
} SketchPinMode;

typedef struct Sketch Sketch;

/* Compile source into something runnable. NULL on failure, with the reason in err - the line
   number included, because a sketch that will not compile is the most common thing that will
   happen to this and "syntax error" on its own is no help to anyone. */
Sketch *sketch_compile(const char *src, char *err, size_t err_size);
void sketch_free(Sketch *s);

/* Back to power-on: setup() not yet run, pins to input, millis() to zero. */
void sketch_reset(Sketch *s);

/* Give the interpreter the pin voltages the solver just produced, in volts, indexed by Arduino
   pin number. Anything the block has no terminal for should be left at 0. */
void sketch_set_pin_voltage(Sketch *s, int pin, double volts);

/* Run the sketch forward to simulated time t (seconds). Returns after a delay() that has not
   expired, after an instruction budget is used up, or when there is nothing left to do -
   whichever comes first, so this is safe to call every solver step. */
void sketch_advance(Sketch *s, double t);

/* What the pins are being driven to. Duty is 0..1: digitalWrite gives 0 or 1, analogWrite gives
   the value it was passed over 255. A pin left as an input has no drive and reads false. */
bool sketch_pin_is_output(const Sketch *s, int pin);
double sketch_pin_duty(const Sketch *s, int pin);
SketchPinMode sketch_pin_mode(const Sketch *s, int pin);
/* True while an analogWrite duty is meant to be switching rather than sitting at a level: the
   block turns this into a real square wave at the PWM frequency so the scope shows what the
   pin is actually doing rather than its average. */
bool sketch_pin_is_pwm(const Sketch *s, int pin);

/* The supply the outputs drive to and the reference analogRead measures against. */
void sketch_set_vcc(Sketch *s, double vcc);

/* Serial.print output, most recent last, for the status bar. NULL when nothing was printed. */
const char *sketch_last_print(const Sketch *s);

/* A runtime fault - a call to a pin that does not exist, a division by zero, a stack overrun.
   The sketch stops and this says why; NULL while it is running normally. */
const char *sketch_error(const Sketch *s);

/* Diagnostics for the suite and the properties panel. */
double sketch_millis(const Sketch *s);
long sketch_loop_count(const Sketch *s);
/* True when the sketch is sitting in a delay() that has not expired yet. */
bool sketch_is_waiting(const Sketch *s);

#endif /* SKETCH_H */
