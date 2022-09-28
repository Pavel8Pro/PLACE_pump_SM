#include <U8glib.h>
#include <AD9833.h>
#include <EEPROM.h>
#include <Rotary.h>


struct Pump_data {
  float flow_rate = 0.00;
  float screw_pitch = 0.00;
  float syringe_ID = 0.00;
  int steps_per_turn = 0;
};

Pump_data pump_data;
U8GLIB_MINI12864 u8g(8, 2, 7, 12, 6);                    //SPI com: SCK = 7, MOSI = 8, CS = 9, A0 = 10, reset = 11
Rotary rotary = Rotary(4,5);


int w = 128;                                              //display width in pixels
int h = 64;                                               //display height in pixels
byte step_pin = A1;
byte dir_pin = A0;

/*
   PUMP/LINEAR MOTION RELATED SETTINGS
*/
//float flow_rate = 0.15;                               //default flow rate mL/h
float flow_rate_inc = 0.05;                                    //flow rate incriment in mL/h
float max_flow_rate = 10.00;                                  //max flow rate mL/h
int max_steps_per_turn = 4096;
float max_screw_pitch = 9.95;
float max_syringe_ID = 9.95;
int microstepping = 64;

float freq = 0;
float fors_freq = 10000;

unsigned char result;

/*
   MENU RELATED VARIABLES AND PINS DECLARATION
*/
char flow_rate_buff[6];                               //buffer to represent the pumping speed in a form a char array
char steps_per_turn_buff[40];                            //|---|
char screw_pitch_buff[6];                              //|---|
char syringe_ID_buff[6];                              //|---|

int cursor_pos = 0;                                             //current cursor positon
int max_cursor_pos = 4;                                          //max cursor position allowed within the screen
int programm_status = 0;                                          //current status of the program: default is 4
int screen_number = 0;                                    //curent screen number

int sw_pin = 3;                                           //encoder button pin
int swState = HIGH;                                       //encoder button initial state

int prog_end_sig = 13; //pin to resieve programm ending status

bool pump_dir = true; // true - up, false - down
bool forsage = true;
bool pumping = false;



char statuses[][14] =                                     //second dimention is the length of the longest string + 1 ( for the '\0' at the end )
{
  "Idle",
  "Set flow rate",
  "Settings",
  "Pumping",
  "Fast UP",
  "Fast DOWN"
};

int cursor_pos_screen0[][4] = {{0, 15, 128, 35}, {0, 53, 10, 10},{10, 53, 10, 10}, {20, 53, 12, 10}, {116, 52, 12, 12}};
int cursor_pos_screen2[][4] = {{9, 16, 80, 12}, {9, 28, 100, 12}, {9, 40, 105, 12}, {94, 1, 34, 12}};
int steps_per_turn_values[5] = {200, 400, 600, 800, 2048};
int steps_per_turn_count = 0;

const uint8_t settings_bitmap[] U8G_PROGMEM = {
  0xc,
  0x18,
  0x11,
  0x33,
  0x7e,
  0xd8,
  0xb0,
  0xe0
};
const uint8_t go_bitmap[] U8G_PROGMEM = {
  0x0,
  0x70,
  0x7c,
  0x7e,
  0x7e,
  0x7c,
  0x70,
  0x0
};
const uint8_t stop_bitmap[] U8G_PROGMEM = {
  0x0,
  0x7e,
  0x7e,
  0x7e,
  0x7e,
  0x7e,
  0x7e,
  0x0
};
const uint8_t up_bitmap[] U8G_PROGMEM = {
  0x0,
  0x18,
  0x3c,
  0x7e,
  0x18,
  0x18,
  0x18,
  0x0
};
const uint8_t down_bitmap[] U8G_PROGMEM = {
  0x0,
  0x18,
  0x18,
  0x18,
  0x7e,
  0x3c,
  0x18,
  0x0
};
const uint8_t ulit_bitmap[] U8G_PROGMEM = {
  0x0,
  0x0,
  0x5,
  0x2,
  0x3a,
  0x7c,
  0xf8,
  0x0 
};
const uint8_t car_bitmap[] U8G_PROGMEM = {
  0x0,
  0x30,
  0x78,
  0x6c,
  0xfe,
  0xff,
  0x42,
  0x0
};

float computeFreq(){
                    // mL/sec               //syringe's cross-area in cm2                         // cm2 per one microstep                                  
  return (pump_data.flow_rate/60/60)/((((3.14*sq(pump_data.syringe_ID*0.1))/4)*(pump_data.screw_pitch*0.1))/(float(pump_data.steps_per_turn)*float(microstepping)));
                                                                   // cm2 per one motor turn
}

void drawScreen0() {
  u8g.firstPage();                                        //begining of the picture loop
  do {
    u8g.setFont(u8g_font_fur30n);                           //set large font
    u8g.drawStr(0, 48, flow_rate_buff);                 //printing the pumping speed

    u8g.setFont(u8g_font_fur11r);                           //set med font
    u8g.drawStr(80, 48, "mL/h");                            //printing flow units
    u8g.drawBitmapP(118, 54, 1, 8, settings_bitmap);
    if (!pumping){
      u8g.drawBitmapP(1, 54, 1, 8, go_bitmap);
    } else {
      u8g.drawBitmapP(1, 54, 1, 8, stop_bitmap);
    }
    if (pump_dir){
      u8g.drawBitmapP(11, 54, 1, 8, up_bitmap);
      
    } else {
      u8g.drawBitmapP(11, 54, 1, 8, down_bitmap);
    }
    if (forsage){
      u8g.drawBitmapP(22, 54, 1, 8, car_bitmap);
    } else {
      u8g.drawBitmapP(22, 54, 1, 8, ulit_bitmap);
    }

    u8g.drawStr(0, 11, statuses[0]);                        //set current status

    u8g.drawFrame(cursor_pos_screen0[cursor_pos][0],
                  cursor_pos_screen0[cursor_pos][1],
                  cursor_pos_screen0[cursor_pos][2],
                  cursor_pos_screen0[cursor_pos][3]);              //draw the frame which represents the cursor position

  } while ( u8g.nextPage() );                           //ending of the picture loop
}

void drawScreen1() {
  u8g.firstPage();                                        //begining of the picture loop
  do {
    u8g.setFont(u8g_font_fur30n);                            //set large font
    u8g.drawStr(0, 48, flow_rate_buff);                 //printing the pumping speed
    u8g.setFont(u8g_font_fur11r);                            //set med font
    u8g.drawStr(80, 48, "mL/h");                            //printing flow units
    u8g.drawStr(0, 11, statuses[1]);                        //set current status
  } while ( u8g.nextPage() );                           //ending of the picture loop
}

void drawScreen2() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_fur11r);
    u8g.drawStr(0, 11, statuses[2]);
    u8g.setFont(u8g_font_helvR08r);
    u8g.drawStr(10, 26, steps_per_turn_buff);
    u8g.drawStr(10, 38, "Syringe ID, mm: ");
    u8g.drawStr(87, 38, syringe_ID_buff);
    u8g.drawStr(10, 50, "Screw pitch, mm: ");
    u8g.drawStr(92, 50, screw_pitch_buff);
    u8g.drawStr(95, 11, "<-Back");

    u8g.drawFrame(cursor_pos_screen2[cursor_pos][0],
                  cursor_pos_screen2[cursor_pos][1],
                  cursor_pos_screen2[cursor_pos][2],
                  cursor_pos_screen2[cursor_pos][3]);              //draw the frame which represents the cursor position

  } while ( u8g.nextPage() );
}

void drawScreen3() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_fur11r);
    u8g.drawStr(0, 11, statuses[2]);
    u8g.setFont(u8g_font_helvR08r);
    u8g.drawBox(cursor_pos_screen2[0][0],
                cursor_pos_screen2[0][1],
                cursor_pos_screen2[0][2],
                cursor_pos_screen2[0][3]);              //draw the frame which represents the cursor position
    u8g.setColorIndex(0);
    u8g.drawStr(10, 26, steps_per_turn_buff);
    u8g.setColorIndex(1);
    u8g.drawStr(10, 38, "Syringe ID, mm: ");
    u8g.drawStr(87, 38, syringe_ID_buff);
    u8g.drawStr(10, 50, "Screw pitch, mm: ");
    u8g.drawStr(92, 50, screw_pitch_buff);
    u8g.drawStr(95, 11, "<-Back");

  } while ( u8g.nextPage() );
}

void drawScreen4() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_fur11r);
    u8g.drawStr(0, 11, statuses[2]);
    u8g.setFont(u8g_font_helvR08r);
    u8g.drawStr(10, 26, steps_per_turn_buff);
    u8g.drawBox(cursor_pos_screen2[1][0],
                cursor_pos_screen2[1][1],
                cursor_pos_screen2[1][2],
                cursor_pos_screen2[1][3]);              //draw the frame which represents the cursor position
    u8g.setColorIndex(0);
    u8g.drawStr(10, 38, "Syringe ID, mm: ");
    u8g.drawStr(87, 38, syringe_ID_buff);
    u8g.setColorIndex(1);
    u8g.drawStr(10, 50, "Screw pitch, mm: ");
    u8g.drawStr(92, 50, screw_pitch_buff);
    u8g.drawStr(95, 11, "<-Back");

  } while ( u8g.nextPage() );
}

void drawScreen5() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_fur11r);
    u8g.drawStr(0, 11, statuses[2]);
    u8g.setFont(u8g_font_helvR08r);
    u8g.drawStr(10, 26, steps_per_turn_buff);
    u8g.drawStr(10, 38, "Syringe ID, mm: ");
    u8g.drawStr(87, 38, syringe_ID_buff);
    u8g.drawBox(cursor_pos_screen2[2][0],
                cursor_pos_screen2[2][1],
                cursor_pos_screen2[2][2],
                cursor_pos_screen2[2][3]);              //draw the frame which represents the cursor position
    u8g.setColorIndex(0);
    u8g.drawStr(10, 50, "Screw pitch, mm: ");
    u8g.drawStr(92, 50, screw_pitch_buff);
    u8g.setColorIndex(1);
    
    u8g.drawStr(95, 11, "<-Back");

  } while ( u8g.nextPage() );
}

void drawScreen6() {
  u8g.firstPage();                                        //begining of the picture loop
  do {
    u8g.drawBox(cursor_pos_screen0[0][0],
                  cursor_pos_screen0[0][1],
                  cursor_pos_screen0[0][2],
                  cursor_pos_screen0[0][3]);              //draw the frame which represents the cursor position
    u8g.setFont(u8g_font_fur30n);                           //set large font
    u8g.setColorIndex(0);
    u8g.drawStr(0, 48, flow_rate_buff);                 //printing the pumping speed
    u8g.setFont(u8g_font_fur11r);                           //set med font
    u8g.drawStr(80, 48, "mL/h");                            //printing flow units
    u8g.setColorIndex(1);
    u8g.drawBitmapP(118, 54, 1, 8, settings_bitmap);
    u8g.drawBitmapP(1, 54, 1, 8, stop_bitmap);
    u8g.drawBitmapP(11, 54, 1, 8, down_bitmap);
    u8g.drawBitmapP(22, 54, 1, 8, ulit_bitmap);
    

    u8g.drawStr(0, 11, statuses[3]);                        //set current status

    u8g.drawFrame(cursor_pos_screen0[1][0],
                  cursor_pos_screen0[1][1],
                  cursor_pos_screen0[1][2],
                  cursor_pos_screen0[1][3]);              //draw the frame which represents the cursor position

  } while ( u8g.nextPage() );                           //ending of the picture loop
}

void drawScreen7() {
  u8g.firstPage();                                        //begining of the picture loop
  do {
    u8g.drawBox(cursor_pos_screen0[0][0],
                  cursor_pos_screen0[0][1],
                  cursor_pos_screen0[0][2],
                  cursor_pos_screen0[0][3]);              //draw the frame which represents the cursor position

    
    u8g.setFont(u8g_font_fur30n);                           //set large font
    u8g.setColorIndex(0);
    u8g.drawStr(0, 48, "*.**");                 //printing the pumping speed
    u8g.setColorIndex(1);
    u8g.setFont(u8g_font_fur11r);                           //set med font
    //u8g.drawStr(80, 48, "mL/h");                            //printing flow units

    u8g.drawBitmapP(118, 54, 1, 8, settings_bitmap);
    u8g.drawBitmapP(1, 54, 1, 8, stop_bitmap);
    u8g.drawBitmapP(11, 54, 1, 8, up_bitmap);
    u8g.drawBitmapP(22, 54, 1, 8, car_bitmap);
    

    u8g.drawStr(0, 11, statuses[4]);                        //set current status

    u8g.drawFrame(cursor_pos_screen0[1][0],
                  cursor_pos_screen0[1][1],
                  cursor_pos_screen0[1][2],
                  cursor_pos_screen0[1][3]);              //draw the frame which represents the cursor position

  } while ( u8g.nextPage() );                           //ending of the picture loop
}

void drawScreen8() {
  u8g.firstPage();                                        //begining of the picture loop
  do {
    u8g.drawBox(cursor_pos_screen0[0][0],
                  cursor_pos_screen0[0][1],
                  cursor_pos_screen0[0][2],
                  cursor_pos_screen0[0][3]);              //draw the frame which represents the cursor position

    
    u8g.setFont(u8g_font_fur30n);                           //set large font
    u8g.setColorIndex(0);
    u8g.drawStr(0, 48, "*.**");                 //printing the pumping speed
    u8g.setColorIndex(1);
    u8g.setFont(u8g_font_fur11r);                           //set med font
    //u8g.drawStr(80, 48, "mL/h");                            //printing flow units

    u8g.drawBitmapP(118, 54, 1, 8, settings_bitmap);
    u8g.drawBitmapP(1, 54, 1, 8, stop_bitmap);
    u8g.drawBitmapP(11, 54, 1, 8, down_bitmap);
    u8g.drawBitmapP(22, 54, 1, 8, car_bitmap);
    

    u8g.drawStr(0, 11, statuses[5]);                        //set current status

    u8g.drawFrame(cursor_pos_screen0[1][0],
                  cursor_pos_screen0[1][1],
                  cursor_pos_screen0[1][2],
                  cursor_pos_screen0[1][3]);              //draw the frame which represents the cursor position

  } while ( u8g.nextPage() );                           //ending of the picture loop
}

void pressed() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) {
    switch (screen_number) {
      case 0:
        switch (cursor_pos) {
          case 0:
            screen_number = 1;
            drawScreen1();
            break;
          case 1:
            pumping = !pumping;
            if (pump_dir){
              forsage = true;
              screen_number = 7;
              digitalWrite(dir_pin, LOW);
              tone(step_pin, fors_freq);
              drawScreen7();
            } else if (forsage) {
              screen_number = 8;
              digitalWrite(dir_pin, HIGH);
              tone(step_pin, fors_freq);
              drawScreen8();
            } else {
              screen_number = 6;
              freq = computeFreq();
              digitalWrite(dir_pin, HIGH);
              tone(step_pin, freq);
              drawScreen6();
            }
            break;
          case 2:
            if (pump_dir){
              pump_dir = false;
            } else if (pump_dir == false){
              pump_dir = true;
              forsage = true;
            }
            drawScreen0();
            break;
          case 3:
            if (pump_dir == false){
              forsage = !forsage;
            } else if (pump_dir == true) {
              forsage = true;
            }
            drawScreen0();
            break;
          case 4:
            max_cursor_pos = 3;
            cursor_pos = 0;
            screen_number = 2;
            drawScreen2();
            break;
        }
        break;
      case 1:
        screen_number = 0;
        drawScreen0();
        EEPROM.put(0, pump_data);
        break;
     case 2:
        switch (cursor_pos){
          case 0:
            screen_number = 3;
            drawScreen3();
            break;
          case 1:
            screen_number = 4;
            drawScreen4();
            break;
          case 2:
            screen_number = 5;
            drawScreen5();
            break;
          case 3:
            max_cursor_pos = 4;
            cursor_pos = 0;
            screen_number = 0;
            drawScreen0();
            EEPROM.put(0, pump_data);
            break;
        }
        break;
    case 3:
      screen_number = 2;
      drawScreen2();
      break;
    case 4:
      screen_number = 2;
      drawScreen2();
      break;
    case 5:
      screen_number = 2;
      drawScreen2();
      break;
    case 6:
      pumping = false;
      screen_number = 0;
      noTone(step_pin);
      drawScreen0();
      EEPROM.put(0, pump_data);
      break;
    case 7:
      pumping = false;
      noTone(step_pin);
      screen_number = 0;
      drawScreen0();
      break;
   case 8:
      pumping = false;
      noTone(step_pin);
      screen_number = 0;
      drawScreen0();
      break;
    }

   
}
last_interrupt_time = interrupt_time; 
}

void setup() {
  delay(3000);
  u8g.setContrast(255);                                   //set max display contrast
  pinMode (sw_pin, INPUT_PULLUP);                         //some pin configuration routine
  pinMode (dir_pin, OUTPUT);
  pinMode (prog_end_sig, INPUT);
  attachInterrupt(digitalPinToInterrupt(sw_pin),
                  pressed,
                  FALLING);

  //Serial.begin(9600);

  EEPROM.get(0, pump_data);   
  if (isnan(pump_data.flow_rate)) {
    pump_data.flow_rate = 0.15;
    pump_data.screw_pitch = 1;
    pump_data.syringe_ID = 5.0;
    pump_data.steps_per_turn = steps_per_turn_values[0];
  }
  dtostrf(pump_data.flow_rate, 3, 2, flow_rate_buff);       //convert pumping speed (number) to an char array for displaying
  dtostrf(pump_data.syringe_ID, 3, 2, syringe_ID_buff);
  dtostrf(pump_data.screw_pitch, 3, 2, screw_pitch_buff);
  sprintf(steps_per_turn_buff, "Steps/turn: %d", pump_data.steps_per_turn);
  drawScreen0();
  
  freq = computeFreq();       
  //Serial.println(freq);

  //attachInterrupt(digitalPinToInterrupt(prog_end_sig), prog_end_routine, RISING);
}                                                                  


void loop() {
    //Serial.println(freq);
    if (digitalRead(prog_end_sig)){
        pumping = false;
        noTone(step_pin);
        screen_number = 0;
        cursor_pos = 1;
        drawScreen0();
    }
    
    result = rotary.process();
    if (result == DIR_CW){
      switch (screen_number) {
        case 0:
            cursor_pos++;
          if (cursor_pos > max_cursor_pos) {
            cursor_pos = 0;
          }
          drawScreen0();
          break;
        case 1:
          pump_data.flow_rate = pump_data.flow_rate + flow_rate_inc;
          if (pump_data.flow_rate > max_flow_rate) {
            pump_data.flow_rate = max_flow_rate;
          }
          dtostrf(pump_data.flow_rate, 3, 2, flow_rate_buff);
          drawScreen1();
          break;
        case 2:
          cursor_pos++;
          if (cursor_pos > max_cursor_pos) {
            cursor_pos = 0;
          }
          drawScreen2();
          break;
        case 3:
          steps_per_turn_count++;
          //pump_data.steps_per_turn++;
          if (steps_per_turn_count>4){
            steps_per_turn_count = 0;
          }
          pump_data.steps_per_turn = steps_per_turn_values[steps_per_turn_count];
          sprintf(steps_per_turn_buff, "Steps/turn: %d", pump_data.steps_per_turn);
          drawScreen3();
          break;
        case 4:
          pump_data.syringe_ID = pump_data.syringe_ID + 0.05;
          if (pump_data.syringe_ID> max_syringe_ID){
            pump_data.syringe_ID = max_syringe_ID;
          }
          dtostrf(pump_data.syringe_ID, 3, 2, syringe_ID_buff);
          drawScreen4();
          break;
        case 5:
          pump_data.screw_pitch = pump_data.screw_pitch + 0.05;
          if (pump_data.screw_pitch > max_screw_pitch){
            pump_data.screw_pitch = max_screw_pitch;
          }
          dtostrf(pump_data.screw_pitch, 3, 2, screw_pitch_buff);
          drawScreen5();
          break;
        case 6:
          pump_data.flow_rate = pump_data.flow_rate + flow_rate_inc;
          if (pump_data.flow_rate > max_flow_rate) {
            pump_data.flow_rate = max_flow_rate;
          }
          dtostrf(pump_data.flow_rate, 3, 2, flow_rate_buff);
          noTone(step_pin);
          freq = computeFreq();
          tone(step_pin, freq);
          //Serial.println(freq);
          drawScreen6();
          break;
      }
    } else if (result == DIR_CCW){
      switch (screen_number) {
        case 0:
            cursor_pos--;
          if (cursor_pos < 0) {
            cursor_pos = max_cursor_pos;
          }
          drawScreen0();
          break;
        case 1:
          pump_data.flow_rate = pump_data.flow_rate - flow_rate_inc;
          if (pump_data.flow_rate < 0) {
            pump_data.flow_rate = flow_rate_inc;
          }
          dtostrf(pump_data.flow_rate, 3, 2, flow_rate_buff);
          drawScreen1();
          break;
        case 2:
          cursor_pos--;
          if (cursor_pos < 0) {
            cursor_pos = max_cursor_pos;
          }
          drawScreen2();
          break;
        case 3:
          steps_per_turn_count--;
          //pump_data.steps_per_turn--;
          if (steps_per_turn_count<0){
            steps_per_turn_count = 4;
          }
          pump_data.steps_per_turn = steps_per_turn_values[steps_per_turn_count];
          sprintf(steps_per_turn_buff, "Steps/turn: %d", pump_data.steps_per_turn);
          drawScreen3();
          break;
         case 4:
          pump_data.syringe_ID = pump_data.syringe_ID - 0.05;
          if (pump_data.syringe_ID < 1.0){
            pump_data.syringe_ID = 1.0;
          }
          dtostrf(pump_data.syringe_ID, 3, 2, syringe_ID_buff);
          drawScreen4();
          break;
        case 5:
          pump_data.screw_pitch = pump_data.screw_pitch - 0.05;
          if (pump_data.screw_pitch < 0.25){
            pump_data.screw_pitch = 0.25;
          }
          dtostrf(pump_data.screw_pitch, 3, 2, screw_pitch_buff);
          drawScreen5();
          break;
        case 6:
          pump_data.flow_rate = pump_data.flow_rate - flow_rate_inc;
          if (pump_data.flow_rate > max_flow_rate) {
            pump_data.flow_rate = max_flow_rate;
          }
          dtostrf(pump_data.flow_rate, 3, 2, flow_rate_buff);
          noTone(step_pin);
          freq = computeFreq();
          tone(step_pin, freq);
          //Serial.println(freq);
          drawScreen6();
          break;
    } 
}

}
