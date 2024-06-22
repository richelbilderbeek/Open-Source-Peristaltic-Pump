// Added a new option, called DoseWait
// In menu item 2, Volume, set the volume

// Hardcoded: number of minutes in DoseWait
const int n_min_dose_wait = 15;

#ifdef NDEBUG
  #define Assert(x) ((void)0)
#else
  #define Assert(x)                  \
  if (!(x))                          \
  {                                  \
    const String msg =               \
      String("E: ") +                \
      String(__LINE__) +             \
      String(", ") +                 \
      String(#x)                     \
    ;                                \
    lcd.setCursor(0, 0);             \
    lcd.print(msg);                  \
    delay(1000);                     \
    abort();                         \
  }
#endif

// include the library code:
#include <LiquidCrystal.h> //https://www.arduino.cc/en/Reference/LiquidCrystal -> LCD control
#include "ClickEncoder.h" //https://github.com/0xPIT/encoder/blob/master/README.md -> Encoder processing (timer based)
#include <TimerOne.h> //required for ClickEncoder.h 
#include <EEPROM.h> //write and read EEPROM (to save and load settings)


//LCD -----------------------------------------------------------------------------------
#define LCD_PIN_D4 8
#define LCD_PIN_D5 9
#define LCD_PIN_D6 10
#define LCD_PIN_D7 11
#define LCD_PIN_RS 13
#define LCD_PIN_EN 12
#define LCD_COLUMNS 16
#define LCD_ROWS 2
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(LCD_PIN_RS, LCD_PIN_EN, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

//ENCODER --------------------------------------------------------------------------------
#define ENCODER_PIN_BUTTON 2
#define ENCODER_PIN_A 3
#define ENCODER_PIN_B 4
ClickEncoder *encoder;
int16_t last, value;

//STEP MOTOR -----------------------------------------------------------------------------
#define MOTOR_STEP_PIN 7
#define MOTOR_DIR_PIN 6
#define STEP_MODE 4 // (1: Full Step, 2: Half Step, 4: Quarter Step, ...)
#define STEPS_PER_FULL_ROT 200 // @ full steps
long delay_us;
long steps;
long step_counter = 0;

//CALIBRATION -----------------------------------------------------------------------------
#define CALIBR_ROTATIONS 30
#define CALIBR_DURATION 30 // seconds
#define CALIBR_DECIMALS 3
const int CALIBR_DECIMAL_CORR = pow(10,CALIBR_DECIMALS);

//SERIAL COMMUNICATION ---------------------------------------------------------------------
#define BAUD 9600
String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete
long vol_uL=0;
long rate_uL_min =0;
int cal=0;
boolean usb_start=0;
char inChar;


//STATE ------------------------------------------------------------------------------------
boolean in_menu=0;
volatile boolean in_action=0;
boolean menu_entered=0;
boolean menu_left=0;

//GENERAL -----------------------------------------------------------------------------------
#define MICROSEC_PER_SEC 1000000

const unsigned int CALIBR_STEPS = CALIBR_ROTATIONS * STEPS_PER_FULL_ROT * STEP_MODE;
const unsigned int CALIBR_DELAY_US = (CALIBR_DURATION * MICROSEC_PER_SEC)/(CALIBR_STEPS*2);


//MENU ---------------------------------------------------------------------------------------
#define MAX_NUM_OF_OPTIONS 4
#define NUM_OF_MENU_ITEMS 10
#define VALUE_MAX_DIGITS 4

// This is the main menu item index, i.e. 0 = start, 2 = volume, etc.
int main_menu_item_index = 0;
int menu_number_2=1;
boolean val_change =0;
double value_dbl;
char value_str[VALUE_MAX_DIGITS+1];

enum menu_type {
  VALUE,
  OPTION,
  ACTION
};

typedef struct 
{
  const char* name_;
  menu_type type; //0: value type, 1:option type, 2:action type
  int value;
  int decimals;
  int lim;
  const char* options[MAX_NUM_OF_OPTIONS];
  const char* suffix;
} menu_item;

menu_item menu[NUM_OF_MENU_ITEMS];
// Maximum index for a menu item
const int menu_items_limit = NUM_OF_MENU_ITEMS - 1;
const int menu_item_idx_calibrate = 7;
const int menu_item_idx_usb_ctrl = 9;


//███ SETUP ████████████████████████████████████████████████████████████████████████████████████████████████████
void setup(){
 Assert(menu_item_idx_calibrate == 7);
 Assert(menu_item_idx_usb_ctrl == 9);
 Assert(menu_item_idx_usb_ctrl < NUM_OF_MENU_ITEMS);
 pinMode(MOTOR_STEP_PIN,OUTPUT); 
 pinMode(MOTOR_DIR_PIN,OUTPUT);
 digitalWrite(MOTOR_DIR_PIN,LOW);
 digitalWrite(MOTOR_STEP_PIN,LOW);

 menu[0].name_ = "Start";
 menu[0].type = ACTION;
 menu[0].value = 0;
 menu[0].lim = 0;
 menu[0].suffix = "RUNNING!";

 menu[1].name_ = "Volume";
 menu[1].type = VALUE;
 menu[1].value = 0;
 menu[1].decimals = 1;
 menu[1].lim = 9999;
 menu[1].suffix="mL";

 menu[2].name_ = "V.Unit:";
 menu[2].type = OPTION;
 menu[2].value = 0;
 menu[2].lim = 3 - 1; // lim = maximum index value for an option, i.e. n_elements - 1
 menu[2].options[0] = "mL";
 menu[2].options[1] = "uL";
 menu[2].options[2] = "rot";

 menu[3].name_ = "Speed";
 menu[3].type = VALUE;
 menu[3].value = 0;
 menu[3].decimals = 1;
 menu[3].lim = 999;
 menu[3].suffix="mL/min";

 menu[4].name_ = "S.Unit:";
 menu[4].type = OPTION;
 menu[4].value = 0;
 menu[4].lim = 3-1; // lim = maximum index value for an option, i.e. n_elements - 1
 menu[4].options[0] = "mL/min";
 menu[4].options[1] = "uL/min";
 menu[4].options[2] = "rpm";

 menu[5].name_ = "Direction:";
 menu[5].type = OPTION;
 menu[5].value = 0;
 menu[5].lim = 2-1; // lim = maximum index value for an option, i.e. n_elements - 1
 menu[5].options[0] = "CW";
 menu[5].options[1] = "CCW";

 menu[6].name_ = "Mode:";
 menu[6].type = OPTION;
 menu[6].value = 0;
 menu[6].lim = 4 - 1; // lim = maximum index value for an option, i.e. n_elements - 1
 menu[6].options[0] = "Dose";
 menu[6].options[1] = "Pump";
 menu[6].options[2] = "Cal.";
 menu[6].options[3] = "DoseWait"; // New setting

 menu[menu_item_idx_calibrate].name_ = "Cal.";
 menu[menu_item_idx_calibrate].type = VALUE;
 menu[menu_item_idx_calibrate].value = 0;
 menu[menu_item_idx_calibrate].decimals = CALIBR_DECIMALS;
 menu[menu_item_idx_calibrate].lim = 20000;
 menu[menu_item_idx_calibrate].suffix="mL";

 menu[8].name_ = "Save Sett.";
 menu[8].type = ACTION;
 menu[8].value = 0;
 menu[8].lim = 0;
 menu[8].suffix = "OK!";

 menu[menu_item_idx_usb_ctrl].name_ = "USB Ctrl";
 menu[menu_item_idx_usb_ctrl].type = ACTION;
 menu[menu_item_idx_usb_ctrl].value = 0;
 menu[menu_item_idx_usb_ctrl].lim = 0;
 menu[menu_item_idx_usb_ctrl].suffix = "ON!";

  // Save all menu items
  for (int i=0; i <= menu_items_limit; i++){
      menu[i].value = eepromReadInt(i*2);
  }
  
  encoder = new ClickEncoder(ENCODER_PIN_B, ENCODER_PIN_A, ENCODER_PIN_BUTTON, 4); //(Encoder A, Encoder B, PushButton)
  encoder->setAccelerationEnabled(false);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  last = 0;
  
  Serial.begin(BAUD);
  inputString.reserve(200);
  // set up the LCD's number of columns and rows:
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  // Print a message to the LCD.
  menu[1].suffix = menu[2].options[menu[2].value];
  if (menu[1].suffix=="uL"){
    menu[1].decimals = 0;
  } else {
    menu[1].decimals = 1;
  }
  if (menu[3].suffix=="uL/min"){
    menu[3].decimals = 0;
  } else {
    menu[3].decimals = 1;
  }
  menu[3].suffix = menu[4].options[menu[4].value];
  update_lcd();
  steps = steps_calc(
    menu[1].value, 
    menu[2].value, 
    menu[menu_item_idx_calibrate].value, 
    menu[1].decimals
  );
  delay_us = delay_us_calc(
    menu[3].value, 
    menu[4].value, 
    menu[menu_item_idx_calibrate].value, 
    menu[3].decimals
  );
  
}



//███ LOOP ████████████████████████████████████████████████████████████████████████████████████████████████████

void loop() {

Assert(main_menu_item_index >= 0);
Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);

// BUTTON HANDLING ////////////////////////////////////////////////////////////////////////////////

ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    switch (b) {
      case ClickEncoder::Clicked:
        if(menu[main_menu_item_index].type == VALUE ||menu[main_menu_item_index].type == OPTION){ // if value or option type
          in_menu =!in_menu;
        }
        if(menu[main_menu_item_index].type == ACTION){ // if action type
          in_action=!in_action;
          step_counter= 0;
        }
        
        if (in_action == true ||in_menu ==true){ //menu entered
          menu_entered = true;
        }
        
        if (in_action == false && in_menu ==false){ //menu left
          menu_left = true;
        }
        break;
        
      case ClickEncoder::DoubleClicked:
          Assert(main_menu_item_index >= 0);
          Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);
          if (menu[main_menu_item_index].type == VALUE){
          menu[main_menu_item_index].value = menu[main_menu_item_index].value + menu[main_menu_item_index].lim/10;
          val_change=true;
          }
        break;
      case ClickEncoder::Held:
          Assert(main_menu_item_index >= 0);
          Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);
          if (menu[main_menu_item_index].type == VALUE){
            menu[main_menu_item_index].value = 0;
            val_change=true;
          }
        break;
      case ClickEncoder::Released:
        break;
        }
  }


/// SETUP ////////////////////////////////////////////////////////////////////////////////

if (menu_entered){
  lcd.blink();
  Assert(main_menu_item_index >= 0);
  Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);
  if (menu[main_menu_item_index].type == ACTION){
    Assert(main_menu_item_index >= 0);
    Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);
    lcd.setCursor((LCD_COLUMNS - strlen(menu[main_menu_item_index].suffix)), 0);
    lcd.print(menu[main_menu_item_index].suffix);
    lcd.setCursor(15, 0);
  }
  if (menu[main_menu_item_index].type == VALUE){
    encoder->setAccelerationEnabled(true);
  }
  menu_entered = false;
}



/// ACTIONS ////////////////////////////////////////////////////////////////////////////////

if (in_action){
  Assert(main_menu_item_index >= 0);
  Assert(main_menu_item_index < NUM_OF_MENU_ITEMS);
  switch (main_menu_item_index){
  case 0: //Start
  if (menu[6].value == 0){ //Dose
    if (dose(steps, delay_us, step_counter)){
      exit_action_menu();
    }
  } else if (menu[6].value == 1){ //Pump
    pump(delay_us);
  } else if (menu[6].value == 2){ //Cal.
    if (dose(CALIBR_STEPS, CALIBR_DELAY_US, step_counter)){
      exit_action_menu();
    }
  } else if (menu[6].value == 3) { // DoseWait
    dose(steps, delay_us, step_counter);
    const int n_sec_dose_wait = n_min_dose_wait * 60;
    const int n_msec_dose_wait = n_sec_dose_wait * 1000;
    delay(n_msec_dose_wait);

  }
  break;
  
  case 8:
   for (int i=0; i <= menu_items_limit; i++){
      eepromWriteInt(i*2,menu[i].value);
   }
   delay(700);
   menu_left = true;
  break;
  
  case menu_item_idx_usb_ctrl:
  while (Serial.available()) {
    inChar = (char)Serial.read();     // get the new byte:
    step_counter = 0;
    if (inChar == 'p'){
      rate_uL_min=Serial.parseInt();
      cal=Serial.parseInt();
      if(cal==0){
        cal = menu[menu_item_idx_calibrate].value;
      }
      delay_us = delay_us_calc(rate_uL_min, 1, cal, 0);
      usb_start=true;
    } else if (inChar == 'd'){
      vol_uL=Serial.parseInt();
      rate_uL_min=Serial.parseInt();
      cal=Serial.parseInt();
      if(cal==0){
        cal = menu[menu_item_idx_calibrate].value;
      }
      steps = steps_calc(vol_uL, 1, cal, 0);
      delay_us = delay_us_calc(rate_uL_min, 1, cal, 0);
      usb_start=true;
    } else if (inChar == 'c'){
      usb_start=true;
    } else if (inChar == 'w'){
      cal=Serial.parseInt();
      menu[menu_item_idx_calibrate].value = cal;
      for (int i=0; i <= menu_items_limit; i++){
      eepromWriteInt(i*2,menu[i].value);
      }
      usb_start=false;
    } else if (inChar == 'x'){
      usb_start=false;
    }
  }
  
  if (usb_start) {
    if(inChar == 'p'){
      pump(delay_us);
    } else if (inChar == 'd') {
      if (dose(steps, delay_us, step_counter)){
        usb_start = false;
      }
    } else if (inChar == 'c'){
      if (dose(CALIBR_STEPS, CALIBR_DELAY_US, step_counter)){
        usb_start = false;
      }
    }
  }
  break;
}

/// MENU (no action) ////////////////////////////////////////////////////////////////////////////////

} else if (!in_action){

if (val_change==true){
  update_lcd();
  val_change==false;
}
  
value += encoder->getValue(); // encoder update

if (!in_menu){ // no menu selected
  // main_menu_item_index = new index
  val_change = encoder_selection(
    main_menu_item_index, 
    menu_number_2, 
    menu_items_limit
  ); //process value change

}else if(in_menu){ // menu selected
  if(menu[main_menu_item_index].type == 0){
    val_change = encoder_value_selection(menu[main_menu_item_index].value, menu[main_menu_item_index].lim);
  } else {
    val_change = encoder_selection(
      menu[main_menu_item_index].value, 
      menu[main_menu_item_index].lim
    );
  }
}

}

/// CLOSE ////////////////////////////////////////////////////////////////////////////////

if (menu_left){
  lcd.noBlink();
  if (menu[main_menu_item_index].type == ACTION){
    exit_action_menu();
  }
  if (menu[main_menu_item_index].type == VALUE){
    encoder->setAccelerationEnabled(false);
  }
  
  if (main_menu_item_index == 2){
    menu[main_menu_item_index-1].suffix = menu[main_menu_item_index].options[menu[main_menu_item_index].value];
      if (menu[1].suffix=="uL"){
        menu[1].decimals = 0;
      } else {
        menu[1].decimals = 1;
       }
  }

  if (main_menu_item_index == 4){
    menu[main_menu_item_index-1].suffix = menu[main_menu_item_index].options[menu[main_menu_item_index].value];
    if (menu[3].suffix=="uL/min"){
      menu[3].decimals = 0;
    } else {
      menu[3].decimals = 1;
    }
  }
  if (main_menu_item_index == 5){ //Change Direction
    if (menu[5].value == 0){
      digitalWrite(MOTOR_DIR_PIN,LOW); 
    }else{
      digitalWrite(MOTOR_DIR_PIN,HIGH);
    }
  }
  steps = steps_calc(
    menu[1].value, 
    menu[2].value, 
    menu[menu_item_idx_calibrate].value, 
    menu[1].decimals
  );
  delay_us = delay_us_calc(
    menu[3].value, 
    menu[4].value, 
    menu[menu_item_idx_calibrate].value, 
    menu[3].decimals
  );
  
  menu_left = false;
}

} 

//███ FUNCTION DECLARATION █████████████████████████████████████████████████████████████████████████████████████████████████

//_____________________________________________________________________________________________

void timerIsr() {
  encoder->service();
}

//_____________________________________________________________________________________________

boolean dose(long _steps, int _delay_us, long & inc) {
      if(inc < _steps){
        digitalWrite(MOTOR_STEP_PIN,HIGH); 
        delayMicroseconds(_delay_us);
        digitalWrite(MOTOR_STEP_PIN,LOW); 
        delayMicroseconds(_delay_us);
        inc++;
        return false;
      } else {
        inc=0;
        return true;
      }
}
//_____________________________________________________________________________________________
/*
void dose_slow(long _steps, int _delay_us, long & inc) {
      if(inc < _steps){
        digitalWrite(MOTOR_STEP_PIN,HIGH); 
        delay(_delay_us/1000); 
        digitalWrite(MOTOR_STEP_PIN,LOW); 
        delay(_delay_us/1000);
        inc++;
      } else {
        inc=0;
        exit_action_menu();
      }
}*/
//_____________________________________________________________________________________________

void pump(int _delay_us) {
        digitalWrite(MOTOR_STEP_PIN,HIGH); 
        delayMicroseconds(_delay_us); 
        digitalWrite(MOTOR_STEP_PIN,LOW); 
        delayMicroseconds(_delay_us);
}
//_____________________________________________________________________________________________

void exit_action_menu(){
   in_action = false;
   lcd.setCursor((LCD_COLUMNS - strlen(menu[main_menu_item_index].suffix)), 0);
   lcd.print("          ");
   lcd.noBlink();
}
//_____________________________________________________________________________________________

long steps_calc(long volume, int unit_mode, int calibr, int decimals){ 
//unit_mode = menu[2].value, volume = menu[1].value, calib = mL/10rot
  
long _steps;
int decimal_corr;
double conv; // rotations/volume
double cal; //volume/rotation

decimal_corr = pow(10,decimals);
cal = calibr;
cal = (cal/CALIBR_ROTATIONS)/CALIBR_DECIMAL_CORR;

  if(unit_mode == 2){ //rot
    conv = 1.0;
  } else if (unit_mode == 1){ //uL
    conv = 1.0/cal/1000;
  } else if (unit_mode == 0){ //mL
    conv = 1.0/cal;
  }

  _steps = STEPS_PER_FULL_ROT * STEP_MODE * conv * volume/decimal_corr;
return _steps;
}
//_____________________________________________________________________________________________

long delay_us_calc(long vol_per_min, int unit_mode, int calibr, int decimals){
  
double d_delay_us;
long _delay_us;
int decimal_corr;
double conv; // rotations/volume
double cal;  

decimal_corr = pow(10,decimals);

cal = calibr;
cal = (cal/CALIBR_ROTATIONS)/CALIBR_DECIMAL_CORR;

  if(unit_mode == 2){ //rot
    conv = 1.0;
  } else if (unit_mode == 1){ //uL
    conv = 1.0/cal/1000;
  } else if (unit_mode == 0){ //mL
    conv = 1.0/cal;
  }
  
  d_delay_us = (1/(STEPS_PER_FULL_ROT * STEP_MODE * conv * vol_per_min/decimal_corr))*60*MICROSEC_PER_SEC/2;
  _delay_us = d_delay_us;
  return _delay_us;
}

//_____________________________________________________________________________________________

void update_lcd(){
    lcd.clear();
//first line LCD ------------------------------------
  lcd.setCursor(0, 0);
  lcd.print(main_menu_item_index);
  lcd.print("|");
  lcd.print(menu[main_menu_item_index].name_);
  if (menu[main_menu_item_index].type == 0){         //if value type
    value_dbl = menu[main_menu_item_index].value;
    value_dbl = value_dbl/pow(10,menu[main_menu_item_index].decimals);
    dtostrf(value_dbl, VALUE_MAX_DIGITS, menu[main_menu_item_index].decimals, value_str );
    lcd.print(" ");
    lcd.print(value_str); //print value
    lcd.print(menu[main_menu_item_index].suffix);
  }  else if(menu[main_menu_item_index].type == 1){  //if option type
    lcd.print(" ");
    lcd.print(menu[main_menu_item_index].options[menu[main_menu_item_index].value]); //print menu[x].option[] of menu[x].value
  } else if(menu[main_menu_item_index].type == 2){   //if action type

  }
  
//second line LCD ------------------------------------
  lcd.setCursor(0, 1);
  lcd.print(menu_number_2);
  lcd.print("|");
  lcd.print(menu[menu_number_2].name_);
  if (menu[menu_number_2].type == 0){         //if value type
    value_dbl = menu[menu_number_2].value;
    value_dbl = value_dbl/pow(10,menu[menu_number_2].decimals);
    dtostrf(value_dbl, VALUE_MAX_DIGITS, menu[menu_number_2].decimals, value_str );
    lcd.print(" ");
    lcd.print(value_str); //print value
    lcd.print(menu[menu_number_2].suffix);
  }  else if(menu[menu_number_2].type == 1){  //if option type
    lcd.print(" ");
    lcd.print(menu[menu_number_2].options[menu[menu_number_2].value]); //print menu[x].option[] of menu[x].value
  } else if(menu[menu_number_2].type == 2){   //if action type

  }
  
  lcd.setCursor(1, 0);
}
//_____________________________________________________________________________________________

boolean encoder_selection(int & x, int lim){ //sub menu
  if (value > last) {
    x++;
    if(x>lim){
      x=0;
    }
    last = value;
    return true;
  }else if(value < last){
    x--;
    if(x<0){
      x=lim;
    }
    last = value;
    return true;
  } else {
    return false;
  }
}
//_____________________________________________________________________________________________

boolean encoder_value_selection(int & x, int lim){ //sub menu
  if (value != last) {
    x = x + value - last;
    if(x>lim){
      x=0;
    }
    if(x<0){
      x=lim;
    }
    last = value;
    return true;
  } else {
    return false;
  }
}
//_____________________________________________________________________________________________

boolean encoder_selection(int & x, int & y, int lim){ //main menu
  if (value > last) {
    x++;
    y++;
    if(x>lim)
    {
      x=0;
    }
    if(y>lim)
    {
      y=0;
    }
    last = value;
    return true;
  }else if(value < last){
    y = main_menu_item_index;
    x--;
    if(x<0)
    {
      x=lim;
    }
    last = value;
    return true;
  }else {
    return false;
  }

}
//_____________________________________________________________________________________________

void eepromWriteInt(int adr, int wert) { 
//http://shelvin.de/eine-integer-zahl-in-das-arduiono-eeprom-schreiben/
byte low, high;

  low=wert&0xFF;
  high=(wert>>8)&0xFF;
  EEPROM.write(adr, low); // dauert 3,3ms 
  EEPROM.write(adr+1, high);
  return;
} //eepromWriteInt
//_____________________________________________________________________________________________

int eepromReadInt(int adr) { 
//http://shelvin.de/eine-integer-zahl-in-das-arduiono-eeprom-schreiben/
byte low, high;

  low=EEPROM.read(adr);
  high=EEPROM.read(adr+1);
  return low + ((high << 8)&0xFF00);
} //eepromReadInt
