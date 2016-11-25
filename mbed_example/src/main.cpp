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
int paddleY=262, paused=0, bY=1, dx=0, ballsLeft=5, score=0, scoreIncrement=1, bounceCount=0, condition=0, gameStart=0,
	paddleX, bX, ballX, ballY, randX, randBool, objX, objY, objWid, randMagicInt, randMagicDur;

int main() {
	srand(time(0)); //Give ball random spawn loc based on time
	ballX = (rand() % 460) + 10;ballY = 30 + (rand() % 40); //Initialise random no between 10-470
	randBool = rand() % 2; // 0 or 1
	randMagicInt = (rand() % 5) + 5; randMagicDur = (rand() % 8) + 2; //Time interval between magic time and rand duration
	if(randBool == 0) { //Random spawn dir for ball
		bX = 1;
	} else {
		bX = -1;
	}
	objX = (rand() % 370) + 10; objY = (rand() % 40) + 75; objWid = (rand() % 160) + 40; //Object dimensions
	paddleX = (rand() % 460) + 10; //Random paddle spawn loc
	// Initialise the display
	screen->fillScreen(WHITE);
	screen->drawRect(0, 0, 480, 280, RED);	
	screen->drawRect(0, 0, 480, 20, RED);
	screen->setTextColor(BLACK, WHITE);
	lm75b.open();
	
	//Initialise ticker and install interrupt handler
	Ticker ticktock;
	ticktock.attach(&timerHandler, 1);
	
	while (true) {
		clock_t start = clock(), diff;
		//Thing to do here();
		diff = clock() - start;
		int msec = diff * 1000 / CLOCKS_PER_SEC;
		screen->setCursor(20, 50);
		printf("Time taken %d seconds %d milliseconds", msec/1000, msec%1000);
		
		
		
		screen->setCursor(20, 7);
		screen->printf("Lives: %d   ", ballsLeft); //Draw info
		screen->setCursor(380, 7);
		screen->printf("Total: %d       ", score);
		if(gameStart==0) { //Gamestart on cntr press
			while(gameStart==0) {
				if(jsPrsdAndRlsd(JCR)) {
					gameStart=1;
				}
			}
		}
		
		screen->fillCircle(ballX, ballY, 5, WHITE); //Draw ball
		ballX	+= bX;	//BallDirection
		ballY += bY;
		screen->fillCircle(ballX, ballY, 5, BLUE);

		screen->fillRect(paddleX, paddleY, 40, 4, WHITE);//Draw paddle
		paddleX += dx; //PaddleDirection
		//paddleX=ballX-20;//Cheats
		screen->fillRect(paddleX, paddleY, 40, 4, BLACK);	
		
		screen->fillRect(objX, objY, objWid, 2, GREEN);
		
		if (ballY <= 29) { //If ball hits roof
			if(bounceCount % 5 == 0 && condition == 1) {
				scoreIncrement++; // If ball bounces 5 times on rally, score increment increases
			}
			condition=1;
			bounceCount++;
			score += scoreIncrement; //Score goes up by the increment			
			bY = 1;
		} else if (ballX <= 6) {
			bX = 1;
		} else if (ballX >= 472) {
			bX = -1;
		}
			
		if (ballY >= paddleY && ballX <= (paddleX+43) && ballX >= (paddleX-3)) { //Paddle collision
			bY = -1;
		} else if ((ballY == objY || ballY == objY) && ballX <= (objX+objWid) && ballX >= (objX-3)) { //Object collision
			bY = bY*-1;
		}
		
		if (ballY >= 277) { //If balls falls off screen
			paused = 1;
			screen->fillCircle(ballX, ballY, 4, WHITE);
			screen->fillRect(paddleX, paddleY, 40, 4, WHITE);
			screen->fillRect(objX, objY, objWid, 2, WHITE);
			if (ballsLeft <= 0) { //Reset game
					if(jsPrsdAndRlsd(JCR)) {
						paused = 0;
						condition = 0;
						score=0;scoreIncrement=1;bounceCount=0;						
						ballX = (rand() % 460) + 10; ballY = 30 + rand() % 40;//Random ball spawn loc							
						paddleX = (rand() % 460) + 10; dx = 0; //Random paddle spawn loc
						ballsLeft=5;
						screen->setCursor(20, 7);
						screen->printf("Lives: %d  ", ballsLeft); //Redraw info
						screen->setCursor(410, 7);
						screen->printf("Total: %d     ", score);						
					}
			}
			while(paused == 1 && ballsLeft != 0) { //If balls goes out of play with lives left
				if(jsPrsdAndRlsd(JCR)) { //Center stick pressed
					objX = (rand() % 370) + 10; objY = (rand() % 40) + 75; objWid = (rand() % 160) + 40;					
					ballX = (rand() % 460) + 10; ballY = 30 + rand() % 40;	
					paddleX = (rand() % 460) + 10; dx = 0;					
					ballsLeft--;
					condition = 0;
					bounceCount = 0;
					scoreIncrement = 1;
					paused = 0;
				}
			}
		}
		paused = 0;
		
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
