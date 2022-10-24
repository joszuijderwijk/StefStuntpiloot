// (c) 2022, Jos Zuijderwijk
#define VERSION 1.0

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27,16,2); // A4 / A5

// BUTTONS
const int DEBOUNCE = 50;
unsigned long buttonTimer = 0;

// prev values
bool buttonLeftState = false;
bool buttonMidState = false;
bool buttonRightState = false;
int prevPot = 0;

// read rotary
const int POT_INTERVAL = 100; // 100 ms
unsigned long potTimer = 0;

// PINS
const int BUTTON_PIN_LEFT = 4; // D4
const int BUTTON_PIN_MID = 3; // D3
const int BUTTON_PIN_RIGHT = 2; // D2
const int MOTOR_PIN_FW = 5;
const int MOTOR_PIN_BW = 6;
const int POT_METER_PIN = A0;

// CUSTOM CHARACTERS
byte playChar[] = {
  B00000, B01000, B01100, B01110,
  B01111, B01110, B01100, B01000
};

byte pauseChar[] = {
  B00000, B11011, B11011, B11011,
  B11011, B11011, B11011, B00000
};


#define ARRAY_SIZE(x) sizeof(x)/sizeof(x[0])

// EEPROM settings
#define DATA_ADDRESS 0

struct saveData{
  bool bets;
  int level;
};

saveData data;

// MENU SETTINGS
int menuPos = 0;
char *Menu[] = {"Default", "Speed", "Random", "Settings"};
char *Settings[] = {"Show bets", "Level"};
enum menu { main, settings, bet, info};
menu currentMenu = main;

int speed = 33;
const int defaultSpeed = 33;
bool start = false;
bool customSpeed = false; // determines whether speed can be changed if stef already started
bool randomSpeed = false; // determines whether the speed is randomly changed
int infoCounter = 0; // press mid 5 times and go to infoscreen

int prevPos = 0; // position in main menu if one returns

// Settings for random run
const int MIN_RANDOM_SPEED = 20; // %
const int MAX_RANDOM_SPEED = 100;
const int MIN_RANDOM_TIME = 2000; // ms
const int MAX_RANDOM_TIME = 12000;
const int BW_INTERVAL = 1000; // 1 s of going backwards
const int BACKWARDS_PROB = 8; // 1/8 chance that stef goes backwards
unsigned long randomTimer = 0;
int randomInterval = 0;

// settings
boolean bets = false; // Show bets at the end
enum difficulty {easy, medium, hard};
difficulty level = medium;

// bets
char *Bets[] = {"sips", "shot", "rutgershot"};
int EasyIntervals[] = {2, 8, 1, 1, 1, 1};   // min/max amount of sips, shots, rutgershots
int EasyProb[] = {8, 1, 1}; // probability distribution
int MedIntervals[] = {5, 10, 1,1, 1,1};
int MedProb[] = {5, 3, 2};
int HardIntervals[] = {5, 15, 1, 2, 1, 1};
int HardProb[] = {7, 1, 2};

// Load menu
void refreshMenu(){

  clearMenuItems();

  // draw >
  if (!start) refreshCursor();

  lcd.setCursor(1, 0);

  switch (currentMenu){
    case main:
      lcd.print(Menu[2 * (menuPos / 2)]);

      // Second menu item
      lcd.setCursor(1,1);
      lcd.print(Menu[2 * (menuPos / 2) + 1]);

      break;
    case settings:
      lcd.print(Settings[0]);
      lcd.setCursor(1, 1);
      lcd.print(Settings[1]);
      break;
  }

}

// clear the two menu items
void clearMenuItems(){
  lcd.setCursor(1,0);
  lcd.print("            ");
  lcd.setCursor(1,1);
  lcd.print("            ");
}

// set speed in the bottom right corner
void refreshSpeed(){
  if (currentMenu != main)
    return;

  int c = 0;

  if (speed < 10)
    c = 1;
  else if (speed == 100)
    c = -1;

  lcd.setCursor(12,1);
  lcd.print("   ");
  lcd.setCursor(13 + c,1);
  lcd.print(String(speed) + "%");
}

// draw start/pause in the upper right corner
void refreshState(){
  lcd.setCursor(15,0);
  lcd.print(" ");
  lcd.createChar(0, start ? playChar : pauseChar);
  lcd.setCursor(15,0);
  lcd.write(0);
}

// redraw >
void refreshCursor(){
    lcd.setCursor(0, 1 - menuPos % 2);
    lcd.print(" ");
    lcd.setCursor(0, menuPos % 2);
    lcd.print(">");
}

// display settings in the settings menu
void refreshSettings(int i = -1){
  if (i <= 0){
    lcd.setCursor(12, 0);
    lcd.print(bets ? "YES" : " NO");
    
  }
  
  if (i <= 1){
    lcd.setCursor(11,1);
    String x;
    if (level == easy)
      x = "EASY";
    else if (level == medium)
      x = " MED";
    else if (level == hard)
      x = "HARD";
    lcd.print(x);
  }
}

// show bets
void showBets() {
  currentMenu = bet;

  String bet;
  int n, r1, r2;
  int *Prob; // points to correct prob dist
  int *Intervals; // points to correct interval

  if (level == easy){
    Prob = EasyProb;
    Intervals = EasyIntervals;
  }
  else if(level == medium){
    Prob = MedProb;
    Intervals = MedIntervals;
  }
  else if (level == hard){
    Prob = HardProb;
    Intervals = HardIntervals;
  }

   // calculate bet
   n = Prob[0] + Prob[1] + Prob[2];
   r1 = random(0, n);  // which bet?

   int b;
   if (r1 <= Prob[0])
     b = 0;
   else if (r1 <= Prob[0] + Prob[1])
     b = 1;
   else
     b = 2;

   bet = Bets[b];
   r2 = random(Intervals[b * 2], Intervals[b*2 + 1] + 1); // how many?
  
  // dirty fix to change 'shot' -> 'shots'
  if (r2 > 1 && b > 0)
    bet += 's';

  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("=== BET ===");
  lcd.setCursor(0,1);
  lcd.print(r2);
  lcd.setCursor(3,1);
  lcd.print(bet);
  lcd.setCursor(15,1);
  lcd.print(">");

}

void setup() 
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_MID, INPUT_PULLUP);
  pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
  pinMode(MOTOR_PIN_FW, OUTPUT);
  pinMode(MOTOR_PIN_BW, OUTPUT);

  randomSeed(analogRead(1));

  lcd.init();
  lcd.backlight();

  refreshMenu();
  refreshState();
  refreshSpeed();

  // load settings from EEPROM
  EEPROM.get(DATA_ADDRESS, data);

  if (data.bets && data.level){
    bets = data.bets;
    level = data.level;
  }
}


// Run!
void run(int speed = defaultSpeed){
  if (speed > 0){
    analogWrite(MOTOR_PIN_FW, 2.55 * speed);
    analogWrite(MOTOR_PIN_BW, 0);
  } else {
    analogWrite(MOTOR_PIN_FW, 0);
    analogWrite(MOTOR_PIN_BW, -2.55 * speed);
  }
  start = true;

  refreshState();
}

// Stop
void stop(){
  start = false;
  analogWrite(MOTOR_PIN_FW, 0);
  analogWrite(MOTOR_PIN_BW, 0);
  refreshState();
}


// Go to next menu item
void next(){
  int n = 0;
  if (currentMenu == main)
    n = ARRAY_SIZE(Menu);
  else if (currentMenu == settings)
    n = ARRAY_SIZE(Settings);

  menuPos = (menuPos + 1) % n;

  if (menuPos % 2 == 0 && n > 2)
    refreshMenu();
  else
    refreshCursor();
}

// Next menu item
void buttonLeft(){
  infoCounter = 0;
  if (!start){
    next();
  }
}

// go back to main menu
void buttonMid(){
  if (currentMenu == settings){

    // Save settings!
    saveData s = saveData();
    s.bets = bets;
    s.level = level;
    EEPROM.put(DATA_ADDRESS, s);

    // Go to menu
    menuPos = 0;
    currentMenu = main;
    lcd.clear();
    refreshMenu();
    refreshState();
    refreshSpeed();
  } else if (currentMenu == main){
    // Pres mid 5 times and go to info screen
    if (infoCounter == 4){
      infoCounter == 0;
      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("Stef v");
      lcd.setCursor(10,0);
      lcd.print(String(VERSION,1));
      lcd.setCursor(0,1);
      lcd.print("jos.contact");
      lcd.setCursor(15,1);
      lcd.print(">");
      currentMenu = info;
    } else {
      infoCounter++;
    }
  }
}

// OK
void buttonRight(){
  infoCounter = 0;

  // Which menu?
  switch (currentMenu) {
    case main:
      // RUN (start / stop)
      if (menuPos == 0){
        if (!start) run();
        else stop();
      }

      // SPEED
      else if (menuPos == 1){
        customSpeed = !customSpeed;
        if (!start) run(speed);
        else stop();
      }
        //RANDOM
      else if (menuPos == 2){
        randomSpeed = !randomSpeed;
        if (!start) start = true;
        else stop();
      }
       
      // SETTINGS
      if (menuPos == 3)
      {
        menuPos = 0;
        currentMenu = settings;
        lcd.clear();
        refreshMenu();
        refreshSettings();
      } 
       // show bets?
      else if (bets && menuPos != 3 && !start) {
        prevPos = menuPos; // save old menuPos
        showBets();
      }
      break;
    case settings:
      // Bets
      if (menuPos == 0){
        bets = !bets;
        refreshSettings(0);
        // Level
      } else if(menuPos == 1){
        switch (level){
          case easy:
            level = medium;
            break;
          case medium:
            level = hard;
            break;
          case hard:
            level = easy;
            break;
          default:
            level = medium;
            break;
        }
        refreshSettings(1);
      }
      break;
    default:
      // go back to main menu;
      menuPos = prevPos;
      currentMenu = main;
      lcd.clear();
      refreshMenu();
      refreshState();
      refreshSpeed();
  }
}


void loop() 
{  

  // Check if any button is pressed
  if (millis() - buttonTimer > DEBOUNCE){
    
    bool left = digitalRead(BUTTON_PIN_LEFT) == LOW;
    bool mid = digitalRead(BUTTON_PIN_MID) == LOW;
    bool right = digitalRead(BUTTON_PIN_RIGHT) == LOW;

    if (buttonLeftState != left){
      if (left) buttonLeft();
      buttonLeftState = left;
      buttonTimer = millis();
    }

    if (buttonMidState != mid){
      if (mid) buttonMid();
      buttonMidState = mid;
      buttonTimer = millis();
    }

    if (buttonRightState != right){
      if (right) buttonRight();
      buttonRightState = right;
      buttonTimer = millis();
    }
    
  }

  // read potmeter
  if (millis() - potTimer > POT_INTERVAL){
    int potVal = analogRead(POT_METER_PIN);
  
    if (prevPot < potVal - 1 || prevPot > potVal + 1 ){
      speed = potVal / 10.23;
      prevPot = potVal;
      refreshSpeed();
      if (start && customSpeed) run(speed);
    }
    potTimer = millis();
  }

  // change random speed
  if (randomSpeed && millis() - randomTimer > randomInterval){
    
    int dir = random(BACKWARDS_PROB) == 1 ? -1 : 1;
    int spd = (MIN_RANDOM_SPEED + random(MAX_RANDOM_SPEED - MIN_RANDOM_SPEED)) * dir;
    run(spd);

    // set new interval
    if (dir == 1)
      randomInterval = MIN_RANDOM_TIME + random(MAX_RANDOM_TIME - MAX_RANDOM_TIME);
    else
      randomInterval = BW_INTERVAL;

    randomTimer = millis();
  }

}
