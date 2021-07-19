#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <EEPROM.h>
#include <avr/eeprom.h>
#include <TimerOne.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

#define TICK 0
#define STORY 1
#define FREEPLAY 2
#define SCORES 3
#define PLAY 4
#define GAMEOVER 5
#define LEFT 6
#define RIGHT 7
#define UP 8
#define DOWN 9
#define SELECT 10
#define N 11
#define T 12
#define M 13
#define LEFT_ARROW 14
#define RIGHT_ARROW 15
#define MENU 16
#define SEQUENCE 17
#define ADDSCORE 18

//CONSTANTS FOR BACKLIGHTING
const int white = 7;
const int red = 1;
const int green = 2;

//EEPROM variables
int startOfList = EEPROM.read(0);
int currentPlayer = startOfList;
uint8_t playerData[6] = {0, 0, 0, 0, 0, 0};
bool gotPlayerData = false;

//global variables for gameover
bool gameOverText = false;
int initialNumber = 3;

//global variables for sequence creation
int opt[4] = {LEFT, RIGHT, UP, DOWN};
int sequence[25];
int score = 0;

//preset freeplay levels and their names
String levels[6] = {"Easy", "Medium", "Hard", "Expert", "Insane", "Custom"};
int levelData[6][3] = {{4, 1500, 2}, {5, 1250, 3}, {7, 1000, 4}, {9, 950, 4}, {15, 750, 4}, {4, 1000, 4}};
int level = 5;

//menu selection variables
int mode = STORY;
int selected = N;
int state = MENU;

//global variables for difficulty
int modulo = 2;
int timer = 1000;
int seqLength = 3;

//global variables for button toggle functions.
bool left_was_pressed = false;
bool right_was_pressed = false;
bool up_was_pressed = false;
bool down_was_pressed = false;
bool select_was_pressed = false;

//global variable to stop rewriting on lcd to stop flickering
bool printed = false;

//global variables for custom characters
byte l_arrow[8] = {
  0b00010,
  0b00110,
  0b01010,
  0b10010,
  0b01010,
  0b00110,
  0b00010,
  0b00000
};
byte r_arrow[8] = {
  0b01000,
  0b01100,
  0b01010,
  0b01001,
  0b01010,
  0b01100,
  0b01000,
  0b00000
};
byte tick[8] = {
  0b00001,
  0b00001,
  0b00010,
  0b00010,
  0b10100,
  0b01000,
  0b00000,
  0b00000
};

//Functions
bool toggleLeft() {
  //Returms true AFTER left button has been pressed and the released
  int btn = lcd.readButtons();
  if (btn & BUTTON_LEFT) {
    Serial.println("left down");
    left_was_pressed = true;
  } else {
    if (left_was_pressed) {
      left_was_pressed = false;
      return true;
    }
  }
  return false;
}

bool toggleRight() {
  //Returms true AFTER right button has been pressed and the released
  int btn = lcd.readButtons();
  if (btn & BUTTON_RIGHT) {
    Serial.println("right down");
    right_was_pressed = true;
  } else {
    if (right_was_pressed) {
      right_was_pressed = false;
      return true;
    }
  }
  return false;
}

bool toggleUp() {
  //Returms true AFTER up button has been pressed and the released
  int btn = lcd.readButtons();
  if (btn & BUTTON_UP) {
    Serial.println("up down");
    up_was_pressed = true;
  } else {
    if (up_was_pressed) {
      up_was_pressed = false;
      return true;
    }
  }
  return false;
}

bool toggleDown() {
  //Returms true AFTER down button has been pressed and the released
  int btn = lcd.readButtons();
  if (btn & BUTTON_DOWN) {
    Serial.println("down down");
    down_was_pressed = true;
  } else {
    if (down_was_pressed) {
      down_was_pressed = false;
      return true;
    }
  }
  return false;
}

bool toggleSelect() {
  //Returms true AFTER select button has been pressed and the released
  int btn = lcd.readButtons();
  if (btn & BUTTON_SELECT) {
    Serial.println("select down");
    select_was_pressed = true;
  } else {
    if (select_was_pressed) {
      select_was_pressed = false;
      return true;
    }
  }
  return false;
}

void assignSequence(int num, int mod) {
  //Assigns a sequence of length num, with mod unique numbers
  for (int i = 0; i < num; i++) {
    sequence[i] = random(6, 6 + mod);
    Serial.println(sequence[i]);
  }
}

int handleKeyPress(int loc) {
  //Gets the button at the given position of the sequence and returns:
  // -1 if another button is pressed
  // 0 if no key is pressed
  // 1 if the correct key is pressed
  int num = sequence[loc];
  switch (num) {
    case 6:
      Serial.println("left");
      if (toggleLeft()) {
        return 1;
      } else if (toggleRight() || toggleUp() || toggleDown()) {
        return -1;
      } else {
        return 0;
      }
      break;
    case 7:
      Serial.println("right");
      if (toggleRight()) {
        return 1;
      } else if (toggleLeft() || toggleUp() || toggleDown()) {
        return -1;
      } else {
        return 0;
      }
      break;
    case 8:
      Serial.println("up");
      if (toggleUp()) {
        return 1;
      } else if (toggleRight() || toggleLeft() || toggleDown()) {
        return -1;
      } else {
        return 0;
      }
      break;
    case 9:
      Serial.println("down");
      if (toggleDown()) {
        return 1;
      } else if (toggleRight() || toggleUp() || toggleLeft()) {
        return -1;
      } else {
        return 0;
      }
      break;
  }
}

String seqToString(int loc) {
  //Converts the numbers assigned to each value into a string for lcd display
  int num = sequence[loc];
  switch (num) {
    case 6:
      return "Left";
      break;
    case 7:
      return "Right";
      break;
    case 8:
      return "Up";
      break;
    case 9:
      return "Down";
      break;
  }
}

void add_score() {
  //Saves a score to EEPROM, adjust other scores positions and handles any overflow scores.
  Serial.println("Score being saved");
  currentPlayer = startOfList;
  uint8_t currentData[6] = {0, 0, 0, 0, 0, 0};
  eeprom_read_block((void*)&currentData, (void*)currentPlayer, 6);
  int counter = 0;
  while (currentData[2] != 255 && counter < 10) {
    eeprom_read_block((void*)&currentData, (void*)currentPlayer, 6);
    if (score > currentData[0] * 256 + currentData[1]) {
      break;
    }
    currentPlayer = currentData[2];
    counter++;
  }
  uint8_t *p = (uint8_t*)malloc(60 * sizeof(uint8_t));
  if (counter < 10) {
    eeprom_read_block((void*)p, (void*)currentPlayer, 60);
    for (int i = 1; i < (10 - counter) * 6; i++) {
      if (i % 6 == 2 && *(p + i) != 255) {
        *(p + i) += 6;
      }
    }
    playerData[2] = currentPlayer + 6;
    eeprom_write_block((void*)&playerData, (void*)currentPlayer, 6);
    currentPlayer += 6;
    eeprom_write_block((void*)p, (void*)currentPlayer, 60);
  }
  currentPlayer = startOfList;
  free(p);
  //this makes sure no more than 10 scores are saved at a time.
  for (int j = 61; j <= 66; j++) {
    EEPROM.update(j, 255);
  }
}

void write_to_lcd(String str, int x = 0, int y = 0) {
  //Writes a string at position x,y on the lcd
  lcd.setCursor(x, y);
  lcd.print(str);
}

void write_special(int num, int x = 0, int y = 0) {
  //Writes a character at position x,y on the lcd
  lcd.setCursor(x, y);
  lcd.write(num);
}

int change_value(int current, int change, int maxValue, int minValue) {
  //returns the new value after change is added, uses
  //minValue and maxValue to wrap round.
  //used for menus to go between selections
  current += change;
  if ( current > maxValue ) {
    current = minValue + (current - maxValue) - 1;
  }
  if ( current < minValue ) {
    current = maxValue - (minValue - current) + 1;
  }
  return current;
}

void clear_screen() {
  //Clears the lcd and makes it so a new screen can be displayed
  printed = false;
  lcd.clear();
}

void clear_EEPROM() {
  //Clears the EEPROM incase being used on a new arduino
  //Adds one score with initials PVN and total score 0
  EEPROM.update(0, 1);
  EEPROM.update(1, 0);
  EEPROM.update(2, 0);
  EEPROM.update(3, 7);
  EEPROM.update(4, 80);
  EEPROM.update(5, 86);
  EEPROM.update(6, 78);
  for (int i = 7; i < 1024; i++) {
    EEPROM.update(i, 255);
  }
  startOfList = EEPROM.read(0);
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  //create a random seed for the sequence generation and create custom characters
  randomSeed(analogRead(0));
  lcd.createChar(LEFT_ARROW, l_arrow);
  lcd.createChar(RIGHT_ARROW, r_arrow);
  lcd.createChar(TICK, tick);
}

void loop() {
  switch (state) {
    case MENU:
      //MENU from which the game type is chosen
      {
        if (mode == FREEPLAY) {
          //display text for freeplay menu option.
          if (!printed) {
            lcd.setBacklight(white);
            write_to_lcd("Freeplay Mode");
            write_to_lcd(levels[level], 0, 1);
            write_special(LEFT_ARROW, 14, 1);
            write_special(RIGHT_ARROW, 15, 1);
            printed = true;
          }

          //change the preset freeplay mode.
          if (toggleRight()) {
            level = change_value(level, 1, 5, 0);
            clear_screen();
          }
          if (toggleLeft()) {
            level = change_value(level, -1, 5, 0);
            clear_screen();
          }

        } else if (mode == STORY) {
          //display story menu option.
          if (!printed) {
            lcd.setBacklight(white);
            write_to_lcd("Story Mode");
            printed = true;
          }

        } else if (mode == SCORES) {
          //display high score menu option.
          if (!printed) {
            lcd.setBacklight(white);
            write_to_lcd("Top 10 Scores");
            printed = true;
          }

        } else {
          //display the reset EEPROM menu option in case the user is on a differnet arduino
          if (!printed) {
            lcd.setBacklight(red);
            write_to_lcd("Reset EEPROM");
            write_to_lcd("will wipe scores", 0, 1);
            printed = true;
          }

        }

        //changes between the options
        if (toggleUp()) {
          mode = change_value(mode, 1, 4, 1);
          clear_screen();
        }
        if (toggleDown()) {
          mode = change_value(mode, -1, 4, 1);
          clear_screen();
        }

        //selects the option and moves to the appropriate screen
        if (toggleSelect()) {
          clear_screen();
          state = mode;
          if (state == STORY) {
            seqLength = 3;
            timer = 1000;
            score = 0;
            modulo = 2;
          } else if (state == FREEPLAY) {
            seqLength = levelData[level][0];
            timer = levelData[level][1];
            modulo = levelData[level][2];
          } else if (state == 4) {
            state = MENU;
            mode = STORY;
            clear_EEPROM();
          }
        }
      }
      break;
    case STORY:
      //Manages the settings/difficulty of story mode so that it progressively gets harder
      {
        if (seqLength < 10) {
          seqLength++;
        } else if (timer > 550) {
          timer -= 25;
        }
        if (seqLength % 2 == 1 && modulo < 4) {
          modulo ++;
        }
        Serial.println("sequence length: " + (String)seqLength);
        assignSequence(seqLength, modulo);
        state = SEQUENCE;
      }
      break;
    case FREEPLAY:
      //Freeplay menu where the user can edit presets and change the difficulty
      //for sequence length(N), timer(T), unique options(M)
      {
        if (selected == N) {
          //displays sequence length option
          if (!printed) {
            write_to_lcd("Sequence Length:");
            write_to_lcd((String)seqLength, 0, 1);
            printed = true;
          }

          //allows the sequence length to be changed from 2 - 25
          if (toggleUp() && seqLength < 25) {
            seqLength++;
            clear_screen();
          }
          if (toggleDown() && seqLength > 2) {
            seqLength--;
            clear_screen();
          }

        } else if (selected == T) {
          //displays the timer option
          if (!printed) {
            write_to_lcd("Timer:");
            write_to_lcd((String)timer, 0, 1);
            printed = true;
          }

          //allows the user to change the timer within the range of 450 - 2500 ms
          if (toggleUp() && timer < 2500) {
            timer += 25;
            clear_screen();
          }
          if (toggleDown() && timer > 450 ) {
            timer -= 25;
            clear_screen();
          }

        } else if ( selected == M) {
          //displays the modulo/(unique options) option
          if (!printed) {
            write_to_lcd("Options:");
            write_to_lcd((String)modulo, 0, 1);
            printed = true;
          }

          //allows the user to change the number of unique options a sequence can include from 2-4
          if (toggleUp() && modulo < 4) {
            modulo++;
            clear_screen();
          }
          if (toggleDown() && modulo > 2) {
            modulo--;
            clear_screen();
          }

        }

        //adds arrows to indicate the menun can be cycled left abd right
        write_special(LEFT_ARROW, 14, 1);
        write_special(RIGHT_ARROW, 15, 1);

        //allows the user to change menu option
        if (toggleRight()) {
          selected = change_value(selected, 1, 13, 11);
          clear_screen();
        }
        if (toggleLeft()) {
          selected = change_value(selected, -1, 13, 11);
          clear_screen();
        }

        //allows the user to start a game
        if (toggleSelect()) {
          assignSequence(seqLength, modulo);
          state = SEQUENCE;
          clear_screen();
        }
      }
      break;
    case PLAY:
      //Screen for when the game is being played
      {
        for (int i = 0; i < seqLength; i++) {
          lcd.setBacklight(white);
          clear_screen();
          //display ticks on screen according to how many the user has got right
          if (i > 0) {
            for (int j = 0; j < i; j++) {
              if (j == 16) {
                lcd.setCursor(0, 1);
              }
              lcd.write(TICK);
            }
          }

          //start a timer for how long the user has to press the button
          long start_s = millis();
          bool correct = false;
          while (millis() - start_s < timer) {
            int corr = handleKeyPress(i);
            if (corr == 1) {
              correct = true;
              break;
            } else if (corr == -1) {
              break;
            }
          }

          //check whether the user got the correct button
          if (correct) {
            score += 5;

            //do a quick green backlight flash to help indicate the user got the correct button
            if ( i != seqLength - 1) {
              lcd.setBacklight(green);
              long t = millis();
              while (millis() - t < 100) {
                Serial.println("Green Flash");
              }
            }

            continue;

          } else {
            //set it so it will go to the gameover screen
            state = GAMEOVER;
            break;
          }
        }

        //checks whether the user should be sent to the gameover screen
        if (state == PLAY) {
          //sends the user back to either FREEPLAY MENU or continue after adjusting the difficulty
          state = mode;
          score += 15;

        } else {
          //sends the user to the gameover screen
          //also sets playerData for story mode
          clear_screen();
          gameOverText = false;
          Serial.println("game lost");
          playerData[3] = 65;
          playerData[4] = 65;
          playerData[5] = 65;

        }
      }
      break;
    case SEQUENCE:
      //the screen which displays the sequence to the user
      {
        clear_screen();
        //displays all items in the sequence with their location in case of a repeating sequence item
        for (int i = 0; i < seqLength; i++) {
          long start_s = millis();
          Serial.println("sequence number " + (String)i);
          int inc = i + 1;
          lcd.print((String)inc + " " + seqToString(i));

          //waits for timer in ms to make sure the user has a chance to read the item.
          while (millis() - start_s < timer) {
            Serial.print(millis() - start_s);
            //waiting
          }

          clear_screen();
        }

        //Display start on the screen for 1000 ms to indicate the user needs to enter the sequence after.
        long start_t = millis();
        lcd.print("START");
        while (millis() - start_t < 1000) {
          //waiting
        }
        clear_screen();
        state = PLAY;
      }
      break;
    case GAMEOVER:
      //screen diasplaying game over message
      {
        //expression to check if the score is in the top 10
        bool top10 = (EEPROM.read(55) * 256 + EEPROM.read(56)) < score || (EEPROM.read(55) * 256 + EEPROM.read(56)) == 65535;

        if (!printed) {

          if (!gameOverText) {
            //display the mode specific game over message for 2.5s
            long start_t = millis();
            if (mode == STORY && top10) {

              lcd.setBacklight(green);
              state = ADDSCORE;
              write_to_lcd("You got a");
              write_to_lcd("new highscore!", 0, 1);

            } else if (mode == STORY) {

              lcd.setBacklight(white);
              write_to_lcd("Sorry not in");
              write_to_lcd("top 10 scores", 0, 1);

            } else if (mode == FREEPLAY) {

              lcd.setBacklight(white);
              write_to_lcd("Maybe " + levels[level]);
              write_to_lcd("was too hard?", 0, 1);

            }
            while (millis() - start_t < 2500) {
              //do nothing
            }
            clear_screen();
            gameOverText = true;
          }

          //display message to indicate select to go to menu
          if (!(mode == STORY && top10)) {
            lcd.setBacklight(red);
            write_to_lcd("Game Over: Press");
            write_to_lcd("Select for menu", 0, 1);
            printed = true;
          }

        }

        //allows the user to press select and go back to the menu screen
        if (toggleSelect()) {
          clear_screen();
          state = MENU;
        }
      }
      break;
    case SCORES:
      //screen for showing the top 10 scores
      {
        if (!printed) {
          //read 6 locations from position *currentPlayer into playerData
          eeprom_read_block((void*)&playerData, (void*)currentPlayer, 6);

          //write the data to the screen
          write_special(playerData[3]);
          write_special(playerData[4], 1);
          write_special(playerData[5], 2);
          write_to_lcd("scored:", 4);
          score = playerData[0] * 256 + playerData[1];
          write_to_lcd((String)score, 0, 1);
          write_special(RIGHT_ARROW, 15, 1);
          printed = true;

        }

        //allows the user to press right to look at the next high score
        if (toggleRight()) {
          currentPlayer = playerData[2];
          if (EEPROM.read(currentPlayer + 2) == 255) {
            //goes back to the start if no more scores to travel to
            currentPlayer = startOfList;
          }
          clear_screen();
        }

        //allows the user to exit back to the menu
        if (toggleSelect()) {
          state = MENU;
          clear_screen();
        }

      }
      break;
    case ADDSCORE:
      //menu for story mode if a high score was made
      {
        lcd.setBacklight(white);
        if (!printed) {
          write_to_lcd("Enter Initials:");
          write_special(playerData[3], 0, 1);
          write_special(playerData[4], 1, 1);
          write_special(playerData[5], 2, 1);
          printed = true;
        }

        //allows the user to change the value of an initial A-Z
        if (toggleUp()) {
          playerData[initialNumber] = change_value(playerData[initialNumber], 1, 90, 65);
          clear_screen();
        }
        if (toggleDown()) {
          playerData[initialNumber] = change_value(playerData[initialNumber], -1, 90, 65);
          clear_screen();
        }

        //allows the user to change which initial is being edited
        if (toggleLeft()) {
          initialNumber = change_value(initialNumber, -1, 5, 3);
        }
        if (toggleRight()) {
          initialNumber = change_value(initialNumber, 1, 5, 3);
        }

        //allows the user to submit a score and return to the menu
        if (toggleSelect()) {
          playerData[0] = score / 256;
          playerData[1] = score % 256;
          add_score();
          state = MENU;
          clear_screen();
        }
      }
      break;
  }
}
