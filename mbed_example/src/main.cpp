#include <mbed.h>
#include <MMA7455.h>
#include <LM75B.h>
#include <display.h>

//Declare output object for LED1
DigitalOut led1(LED1);

// Initialise Joystick   
typedef enum {JLT = 0, JRT, JUP, JDN, JCR} btnId_t;
static DigitalIn jsBtns[] = {P5_0, P5_4, P5_2, P5_1, P5_3}; // LFT, RGHT, UP, DWN, CTR
bool jsPrsdAndRlsd(btnId_t b);

//Input object for the potentiometer
AnalogIn pot(p15);
float potVal = 0.0;
  
//Object to manage the accelerometer
MMA7455 acc(P0_27, P0_28);
bool accInit(MMA7455& acc); //prototype of init routine
int32_t accVal[3];

//Object to manage temperature sensor
LM75B lm75b(P0_27, P0_28, LM75B::ADDRESS_1);
float tempVal = 0.0;

Display *screen = Display::theDisplay();
    //This is how you call a static method of class Display
    //Returns a pointer to an object that manages the display screen 

//Timer interrupt and handler
void timerHandler(); //prototype of handler function
int tickCt = 0;

//Drawing coordinates
int x = 240, y = 120, dx = 1, dy = 0;


int main() {
  // Initialise the display
  screen->fillScreen(WHITE);
  screen->setTextColor(BLACK, WHITE);

  // Initialise accelerometer and temperature sensor
  screen->setCursor(2,82);
  if (accInit(acc)) {
		screen->printf("Accelerometer initialised");
	} else {
    screen->printf("Could not initialise accelerometer");
  }
  lm75b.open();
  
  //Initialise ticker and install interrupt handler
  Ticker ticktock;
  ticktock.attach(&timerHandler, 1);
  
  screen->drawCircle(200, 100, 80, BLUE);
  
  while (true) {
    potVal = pot.read();
    tempVal = lm75b.temp();
    acc.read(accVal[0], accVal[1], accVal[2]);

    screen->setCursor(2,2);
    screen->printf("Pot = %1.2f, Temp = %03.1f", potVal, tempVal);
    screen->setCursor(162,2);
    screen->printf(
      "Acc = (%05d, %05d, %05d)", accVal[0], accVal[1], accVal[2]); 

    screen->setCursor(2,12);
    screen->printf("Tick count = %d, x = %d, y = %d", tickCt, x, y);
        
    //draw a moving blob on display - steerable using joystick
    screen->fillCircle(x, y, 4, WHITE);
    x += dx;
    y += dy;
    screen->fillCircle(x, y, 4, RED);
    if (jsPrsdAndRlsd(JUP)) {       
      dx = 0; dy = -1;
    } else if (jsPrsdAndRlsd(JDN)) {
      dx = 0; dy = 1;
    } else if (jsPrsdAndRlsd(JLT)) { // NB response to JS L, R reversed
      dx = 1; dy = 0;                 //   because JS is upside-down
    } else if (jsPrsdAndRlsd(JRT)) {
      dx = -1; dy = 0;
    } else if (jsPrsdAndRlsd(JCR)) {
      dx = 0; dy = 0;
    }

    wait(0.005); //5 milliseconds
  }
}

bool accInit(MMA7455& acc) {
  bool result = true;
  if (!acc.setMode(MMA7455::ModeMeasurement)) {
    // screen->printf("Unable to set mode for MMA7455!\n");
    result = false;
  }
  if (!acc.calibrate()) {
    // screen->printf("Failed to calibrate MMA7455!\n");
    result = false;
  }
  // screen->printf("MMA7455 initialised\n");
  return result;
}

//Definition of timer interrupt handler
void timerHandler() {
  tickCt++;
  led1 = !led1; //every tick, toggle led1
}

/* Definition of Joystick press capture function
 * b is one of JLEFT, JRIGHT, JUP, JDOWN - from enum, 0, 1, 2, 3
 * Returns true if this Joystick pressed then released, false otherwise.
 *
 * If the value of the button's pin is 0 then the button is being pressed,
 * just remember this in savedState.
 * If the value of the button's pin is 1 then the button is released, so
 * if the savedState of the button is 0, then the result is true, otherwise
 * the result is false. */
bool jsPrsdAndRlsd(btnId_t b) {
	bool result = false;
	uint32_t state;
	static uint32_t savedState[4] = {1,1,1,1};
        //initially all 1s: nothing pressed
	state = jsBtns[b].read();
  if ((savedState[b] == 0) && (state == 1)) {
		result = true;
	}
	savedState[b] = state;
	return result;
}
