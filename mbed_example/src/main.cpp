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
/*************************************************************************/
//Drawing coordinates
int paddleY=262, cond=0, bY=1, dx=0, ballsLeft=5, score=0, scoreIncrement=1, bounceCount=0, gameState=0,
	paddleX, bX, ballX, ballY, randX, randBool;

int main() {
	srand(time(0)); //Give ball random spawn loc baed on time
	ballX = (rand() % 460) + 10; //Initialise random no between 10-470
	ballY = 30 + (rand() % 40); 
	randBool = rand() % 2; // 0 or 1
	if(randBool == 0) { //Random spawn dir for ball
		bX = 1;
	} else {
		bX = -1;
	}
	paddleX = (rand() % 460) + 10; //Random paddle spawn loc
	// Initialise the display
	screen->fillScreen(WHITE);
	screen->drawRect(0, 0, 480, 280, BLACK);	
	screen->drawRect(0, 0, 480, 20, BLACK);
	screen->setTextColor(BLACK, WHITE);
	lm75b.open();
	
	//Initialise ticker and install interrupt handler
	Ticker ticktock;
	ticktock.attach(&timerHandler, 1);
	
	while (true) {
		gameState = 0;
		screen->setCursor(20, 7);
		screen->printf("Lives: %d", ballsLeft); //Draw info
		screen->setCursor(410, 7);
		screen->printf("Total: %d", score);
		
		screen->fillCircle(ballX, ballY, 5, WHITE);
		ballX	+= bX;	//BallDirection
		ballY += bY;
		screen->fillCircle(ballX, ballY, 5, BLUE);

		screen->fillRect(paddleX, paddleY, 40, 4, WHITE);
		paddleX += dx; //PaddleDirection
		screen->fillRect(paddleX, paddleY, 40, 4, BLACK);	
		
		if (ballY <= 26) {
			if(bounceCount % 5 == 0 && score != 0) {
				scoreIncrement++; // If ball bounces 5 times on rally, score increment increases
			}
			bounceCount++;
			score += scoreIncrement; //Score goes up by the increment			
			bY = 1;
		} else if (ballX <= 6) {
			bX = 1;
		} else if (ballX >= 472) {
			bX = -1;
		}
			
		if (ballY >= paddleY && ballX <= (paddleX+45) && ballX >= (paddleX-5)) { //Paddle collision
			bY = -1;
		}
		if (ballY >= 277) {
			screen->fillCircle(ballX, ballY, 4, WHITE);
			while(cond == 0) {
				if(jsPrsdAndRlsd(JCR)) { //Center stick pressed					
					ballX = (rand() % 460) + 10; ballY = 30 + rand() % 40;					
					screen->fillRect(paddleX, paddleY, 40, 5, WHITE); //Reset paddle
					paddleX = 240, dx = 0;
					ballsLeft--;
					bounceCount = 0;
					scoreIncrement = 1;
					cond = 1;
				}
				if(ballsLeft <= 0) {
					screen->setCursor(20, 7);
					screen->printf("Lives: %d", ballsLeft);
					while(gameState==0) {
						if(jsPrsdAndRlsd(JCR)) {
						score=0;scoreIncrement=1;bounceCount=0;ballsLeft=5;
						ballX = (rand() % 460) + 10; ballY = 30 + rand() % 40;	
						paddleX = (rand() % 460) + 10; dx = 0; //Random paddle spawn loc
						//Reset pos
						screen->fillRect(paddleX, paddleY, 40, 5, BLACK);
						screen->fillCircle(ballX, ballY, 4, BLUE);
						//Clear screen to default 						
						gameState=1; //Return to game
						}					
					}
				}
			}
		}
				cond = 0;
		
		if (jsPrsdAndRlsd(JLT)) { 
			dx = 2;
		} else if (jsPrsdAndRlsd(JRT)) { //Joystick paddle movement
			dx = -2;
		}
		
		if(paddleX >= 440) {
			dx = 0; paddleX = 439;
		} else if(paddleX <= 0) { //Fixes wall collision
			dx = 0; paddleX = 1;
		}
		
		wait(0.005);	
	}//End loop
}//End main
	/********************************************************************************/	

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
//LiamMorgan 17:00 23/11