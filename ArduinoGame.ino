#include <LiquidCrystal.h>

LiquidCrystal lcd(4, 5, 6, 7, 8, 9); // initialise the LCD display with the pins it is connected to

const int DIAL_PIN = A0; // analog pin the dial is connected to
const int MAX_DIAL_VALUE = 1023;

// for 4 difficulties, calculate the boundary of the dial output at which the difficulty changes
const int DIFFICULTY_BOUNDARY = (MAX_DIAL_VALUE - 3) / 4;

const int BUTTON_PIN = 2;
const int TILT_SENSOR_PIN = 3;

const int MAX_SCORE = 99; // score required to win the game

const int MAX_QUEUE_LENGTH = 13;
const int STARTING_COLUMN = 16; // the value of LCD column that all new spikes are given when created

int delayTime; // controls the speed at which the game runs

// variables used to calculate the time an interrupt function is run for, used to debounce the button and tilt sensor
int timeInButtonInterrupt;
int prevTimeInButtonInterrupt;

int timeInTiltSensorInterrupt;
int prevTimeInTiltSensorInterrupt;

// struct used to store the location of the player and the players score
struct player {
    int row;
    int column;
    int score;
};

// struct used to store the location of a spike
struct spike {
    int row;
    int column;
};

struct player player;

// initialise an array of spikes to act as a queue for the spikes in the game
struct spike spikeQueue[MAX_QUEUE_LENGTH];

// for the queue
int front = -1;
int rear = -1;

// custom characters for the players and spikes that can be interpreted and printed by the LCD
byte bottomPlayer[8] = {
    B00000,
    B11111,
    B10101,
    B10101,
    B11111,
    B01010,
    B01010,
    B11011
};

byte topPlayer[8] = {
    B11011,
    B01010,
    B01010,
    B11111,
    B10101,
    B10101,
    B11111,
    B00000
};

byte topSpike[8] = {
    B11111,
    B11111,
    B01110,
    B01110,
    B00100,
    B00100,
    B00000,
    B00000
};

byte bottomSpike[8] = {
    B00000,
    B00000,
    B00100,
    B00100,
    B01110,
    B01110,
    B11111,
    B11111
};

void setup() {
    lcd.begin(16, 2); // specifies the LCD to have 16 columns and 2 rows

    // using analogRead on an unconnected pin allows for the random function to differ each time the program runs
    randomSeed(analogRead(1));

    // create the characters so that they can be displayed by the LCD
    lcd.createChar(0, topPlayer);
    lcd.createChar(1, bottomPlayer);
    lcd.createChar(2, topSpike);
    lcd.createChar(3, bottomSpike);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonFunction, FALLING);
    attachInterrupt(digitalPinToInterrupt(TILT_SENSOR_PIN), tiltSensorFunction, CHANGE);

    timeInButtonInterrupt = 0;
    prevTimeInButtonInterrupt = 0;

    timeInTiltSensorInterrupt = 0;
    prevTimeInTiltSensorInterrupt = 0;

    player.column = 3;
    player.row = 1;
    player.score = 0;
}

void loop() {
    if(checkGameEnd(spikeQueue[front])) {
        gameOver();
    } else if(player.score == MAX_SCORE) {
        gameWon();
    } else {
        setDifficulty();

        // print the players score in the top left of the display
        lcd.setCursor(0, 0);
        lcd.print(player.score);

        // draw the player
        lcd.setCursor(player.column, player.row);
        lcd.write(byte(player.row));

        // generate a new spike every other two columns,
        // if a gap of two columns is not present, do not generate a spike
        if(spikeQueue[rear - 1].row == -1 && spikeQueue[rear].row == -1) {
            struct spike newSpike;
            newSpike.column = STARTING_COLUMN;
            newSpike.row = random(2); // the row of the spike is randomized
            enqueue(newSpike);
        } else {
            struct spike newSpike;
            newSpike.column = STARTING_COLUMN;
            newSpike.row = -1;
            enqueue(newSpike);
        }

        for(int i = 0; i < MAX_QUEUE_LENGTH; i++) {
                if(spikeQueue[i].column >= player.column) {
                // move the spike one column towards the player as long as the spike is not beyond the player
                spikeQueue[i].column--;

                // draw the spike with the correct orientation
                if(spikeQueue[i].row == 0) {
                    lcd.setCursor(spikeQueue[i].column, spikeQueue[i].row);
                    lcd.write(byte(2));
                } else if (spikeQueue[i].row == 1) {
                    lcd.setCursor(spikeQueue[i].column, spikeQueue[i].row);
                    lcd.write(byte(3));
                }
            }
        }

        // if the player has successfully avoided the spike at the front of the queue, increase their score
        if(checkPointScored(spikeQueue[front])) {
            player.score++;
        }

        delay(delayTime);

        lcd.clear();
    }
}

void buttonFunction() {
    timeInButtonInterrupt = millis();

    // caluclate the time the interrupt function has run for and check if it is slower than 250ms
    // if this time is not slower, then consider this a bounce and ignore
    if(timeInButtonInterrupt - prevTimeInButtonInterrupt >= 250) {
        // if  the game has not ended, switch the players row to 0 if they are not already on that row
        if(!(checkGameEnd(spikeQueue[front]) || (player.score == MAX_SCORE))) {
            if(player.row != 0) {
                player.row = 0;
            }
        }

        prevTimeInButtonInterrupt = timeInButtonInterrupt;
    }
}

void tiltSensorFunction() {
    timeInTiltSensorInterrupt = millis();

    // caluclate the time the interrupt function has run for and check if it is slower than 250ms
    // if this time is not slower, then consider this a bounce and ignore
    if(timeInTiltSensorInterrupt - prevTimeInTiltSensorInterrupt >= 250) {
        // if the game has not ended, switch the players row to 1 if they are not already on that row
        if(!(checkGameEnd(spikeQueue[front]) || (player.score == MAX_SCORE))) {
            if(player.row != 1) {
                player.row = 1;
            }
        }

        prevTimeInTiltSensorInterrupt = timeInTiltSensorInterrupt;
    }
}

// retuns true if the queue is not empty and the player has collided with the spike passed as an argument
bool checkGameEnd(struct spike leadingSpike) {
    return (!queueEmpty()) && (leadingSpike.column <= player.column && leadingSpike.row == player.row);
}

// returns true if the queue is not empty and the player has avoided the spike passed as an argument
bool checkPointScored(struct spike leadingSpike) {
    return (!queueEmpty())
        && (leadingSpike.column <= player.column && (leadingSpike.row != player.row && leadingSpike.row != -1));
}

// prints the game over message and the players final score on the LCD
void gameOver() {
    lcd.setCursor(3, 0);
    lcd.print("GAME OVER!");

    lcd.setCursor(4, 1);
    lcd.print("score:");
    lcd.print(player.score);
}

// prints the game won message on the LCD
void gameWon() {
    lcd.setCursor(4, 0);
    lcd.print("YOU WIN!");

    lcd.setCursor(4, 1);
    lcd.print("score:");
    lcd.print(player.score);
}

// uses the value from the potentiometer to change the value of the delayTime variable based on
// the calculated difficulty boundaries
void setDifficulty() {
    int dialValue = analogRead(DIAL_PIN);

    if(dialValue >= 0 && dialValue <= DIFFICULTY_BOUNDARY) {
        delayTime = 1500; // easiest difficulty
    } else if(dialValue > DIFFICULTY_BOUNDARY && dialValue <= 2 * DIFFICULTY_BOUNDARY) {
        delayTime = 1000;
    } else if(dialValue > 2 * DIFFICULTY_BOUNDARY && dialValue <= 3 * DIFFICULTY_BOUNDARY) {
        delayTime = 500;
    } else if(dialValue > 3 * DIFFICULTY_BOUNDARY && dialValue <= MAX_DIAL_VALUE) {
        delayTime = 300; // hardest difficulty
    } else {
        delayTime = 2000;
    }
}

// returns true if the queue is full
bool queueFull() {
    return (front == rear + 1) || (front == 0 && rear ==  MAX_QUEUE_LENGTH - 1);
}

// returns true if the queue is empty
bool queueEmpty() {
    return front == -1;
}

// if the queue is not full, calculate the new rear pointer of the queue and insert the spike at this position,
// otherwise, remove the front spike and run enqueue again
void enqueue(struct spike s) {
    if(!queueFull()) {
        if(front == -1) {
            front = 0;
        }

        rear = (rear + 1) % MAX_QUEUE_LENGTH;

        spikeQueue[rear] = s;
    } else {
        dequeue();
        enqueue(s);
    }
}

// if the queue is not empty, remove the spike at the front of the queue
// return the spike at this position as well
struct spike dequeue() {
    struct spike s;

    if(!queueEmpty()) {
        s = spikeQueue[front];

        if(front == rear) {
            front = -1;
            rear = -1;
        } else {
            front = (front + 1) % MAX_QUEUE_LENGTH;
        }
    }

    return s;
}
