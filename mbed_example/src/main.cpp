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
int paddleX = 240, paddleY = 260, dx = 0, ballX = 100, ballY = 100, bX = 1, bY = 1, cond = 0;



int main() {
  // Initialise the display
  screen->fillScreen(BLACK);
	screen->drawRect(0, 0, 480, 280, WHITE);
	screen->drawRect(0, 0, 480, 20, WHITE);
  screen->setTextColor(WHITE, BLACK);
  lm75b.open();
  
  //Initialise ticker and install interrupt handler
  Ticker ticktock;
  ticktock.attach(&timerHandler, 1);
  
  while (true) {
		
		screen->setCursor(30, 5);
		screen->printf("RandX: %d, RandY: %d", rand(), rand()); //Draw random number on screen
        
    screen->fillCircle(ballX, ballY, 4, BLACK);
		ballX	+= bX;	//BallDirection
		ballY += bY;
		screen->fillCircle(ballX, ballY, 4, BLUE);
		screen->drawRect(paddleX, paddleY, 40, 5, BLACK);
		paddleX += dx; //PaddleDirection
		screen->drawRect(paddleX, paddleY, 40, 5, WHITE);	
		
		if(paddleX <= 1 || paddleX >=  439) {
			dx = 0;
		}
		
		if (ballY <= 26) {
			bY = 1;
		} else if (ballX <= 6) {
			bX = 1;
		} else if (ballX >= 474) {
			bX = -1;
		}
			
		if (ballY >= paddleY && ballX <= (paddleX+45) && ballX >= (paddleX-5)) { //Paddle collision
			bY = -1;
		}
		if (ballY >= 275) {
			screen->fillCircle(ballX, ballY, 4, BLACK);
			while(cond == 0) {
				if(jsPrsdAndRlsd(JCR)) {
					cond++;
					ballX = 100, ballY = 70;
				} else if (ballY < 260) {
					cond--;
				}
			}
		}
		
    if (jsPrsdAndRlsd(JLT)) { //Joystick controls
      dx = 1;
    } else if (jsPrsdAndRlsd(JRT)) {
      dx = -1;
    } else if (jsPrsdAndRlsd(JCR)) {
			screen->fillCircle(ballX, ballY, 4, BLACK);
			ballX = 100, ballY = 70;
			screen->fillCircle(ballX, ballY, 4, BLUE);
    }
		
		if(paddleX >= 440) {
			dx = 0; paddleX = 439;
		} else if(paddleX <= 0) {
			dx = 0; paddleX = 1;
		}
		
}
		
    wait(0.0005); //5 milliseconds
  }//End while loop

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
//LiamMorgan