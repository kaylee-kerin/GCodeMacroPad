
#include <stdarg.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306_STM32.h>

#define OLED_RESET PB5
Adafruit_SSD1306 display(OLED_RESET);

#include "HardwareTimer.h"


#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define BUTTON_STATE_RELEASE 0
#define BUTTON_STATE_PRESS   1
#define BUTTON_STATE_HOLD    2
#define BUTTON_HOLD_REPEAT_MS 400

#define PANEL1_SW1   0
#define PANEL1_SW2   1
#define PANEL1_SW3   2
#define PANEL1_SW4   3
#define PANEL1_SW5   4

#define PANEL2_SW1   5
#define PANEL2_SW2   6
#define PANEL2_SW3   7
#define PANEL2_SW4   8
#define PANEL2_SW5   9

#define PANEL1_SW6   10
#define PANEL1_SW7   11
#define PANEL1_SW8   12
#define PANEL1_SW9   13
#define PANEL1_SW10  14

#define PANEL2_SW6   15
#define PANEL2_SW7   16
#define PANEL2_SW8   17
#define PANEL2_SW9   18
#define PANEL2_SW10  19

#define PANEL1_SW11  20
#define PANEL1_SW12  21
#define PANEL1_SW13  22
#define PANEL1_SW14  23
#define PANEL1_SW15  24

#define PANEL2_SW11  25
#define PANEL2_SW12  26
#define PANEL2_SW13  27
#define PANEL2_SW14  28
#define PANEL2_SW15  29


#define GROUP_D_PER_PULSE   4,PANEL1_SW12,PANEL1_SW14,PANEL1_SW13,PANEL1_SW11
#define GROUP_D_PER_PULSE_0005   PANEL1_SW14
#define GROUP_D_PER_PULSE_0010   PANEL1_SW13
#define GROUP_D_PER_PULSE_0100   PANEL1_SW12
#define GROUP_D_PER_PULSE_0500   PANEL1_SW11

#define GROUP_AXIS_SELECT   4,PANEL1_SW6,PANEL1_SW7,PANEL1_SW8,PANEL1_SW9

#define GROUP_AXIS_SELECT_X      PANEL1_SW6
#define GROUP_AXIS_SELECT_Y      PANEL1_SW7
#define GROUP_AXIS_SELECT_Z      PANEL1_SW8
#define GROUP_AXIS_SELECT_A      PANEL1_SW9

#define GROUP_FEED_OVERRIDE 6,PANEL2_SW2,PANEL2_SW1,PANEL2_SW3,PANEL2_SW4,PANEL2_SW6,PANEL2_SW11
    

#define ALARM_LED  PANEL2_SW5

//Encoder stuff

//Pulses per revolution
#define PPR   1000

HardwareTimer timer(2);

HardwareTimer matrix(1);

void func(){
if (timer.getDirection()){
  //timer.setCount(0);
} else{
 // timer.setCount(PPR-1);
}
}



// 2-dimensional array of row pin numbers:
#define ROWCOUNT 10
const int row[ROWCOUNT] = {
 PB9,PB8,PB1,PB10,PB11,PA8,PB15,PB14,PB13,PB12
};

// 2-dimensional array of column pin num bers:
const int col_led[3] = {
  PA4,PA3,PA2
};

const int col_in[3] = {
  PA7,PA6,PA5
};


// cursor position:
int x = 0;
int y = 0;

char  inbuffer[128];
int inbuffer_idx = 0;

char statusMessage[81] = "Online";

void setup() {
  
  inbuffer_idx=0;
  memset(inbuffer,0,sizeof(inbuffer));

  
  pinMode(33, OUTPUT);
 
  Serial1.begin(115200);
//  Serial1.setTimeout(2);
  
  //define the Timer channels as inputs. 
  pinMode(PA0, INPUT_PULLUP);  //channel A
  pinMode(PA1, INPUT_PULLUP);  //channel B
 
  pinMode(PB0, INPUT_PULLUP);  //jog wheel button
 
  for(x=0;x<3;x++){
     pinMode(col_led[x], OUTPUT);
     pinMode(col_in[x], OUTPUT);
     digitalWrite(col_led[x], LOW);
  }
  for(y=0;y<ROWCOUNT;y++){
     pinMode(row[y], INPUT);
     digitalWrite(row[y], LOW);
  }


//configure timer as encoder
  timer.setMode(0, TIMER_ENCODER); //set mode, the channel is not used when in this mode. 
  timer.pause(); //stop... 
  timer.setPrescaleFactor(1); //normal for encoder to have the lowest or no prescaler. 
  timer.setOverflow(PPR);    //use this to match the number of pulse per revolution of the encoder. Most industrial use 1024 single channel steps. 
  timer.setCount(500);          //reset the counter. 
  timer.setEdgeCounting(TIMER_SMCR_SMS_ENCODER1); //or TIMER_SMCR_SMS_ENCODER1 or TIMER_SMCR_SMS_ENCODER2. This uses both channels to count and ascertain direction. 
//  timer.attachInterrupt(0, func); //channel doesn't mean much here either.  
  timer.resume();                 //start the encoder... 

  
  matrix.pause();  
  matrix.setPeriod(10);
  matrix.setMode(TIMER_CH1,TIMER_OUTPUT_COMPARE);
  matrix.setCompare(TIMER_CH1,1);
  matrix.attachInterrupt(TIMER_CH1,int_matrix); 
  matrix.refresh();
  matrix.resume();
  
  
  group_init(GROUP_D_PER_PULSE);
  group_init(GROUP_AXIS_SELECT);
  group_init(GROUP_FEED_OVERRIDE);

  //sleep before we start up the display, give it time to finish it's thing.  (Otherwise it just stays blank...)
  delay(200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);  // initialize with the I2C addr 0x3D (for the 128x64)
      
}

//Support variables.
unsigned long interval=0; //variable for status updates... 
char received = 0;


volatile bool led_state[3*ROWCOUNT];
volatile bool button_state[3*ROWCOUNT];
volatile bool button_pressed[3*ROWCOUNT];
volatile bool button_released[3*ROWCOUNT];

volatile char led = 0;

void int_matrix(){
  scan_matrix();
}

uint32_t last_repeat = 0;

void scan_matrix(){
//button scanning
   uint8_t j=0;
   x = led/ROWCOUNT;
   y = led%ROWCOUNT;

   
   //drive the column high, so we can read the button/light the LED
   digitalWrite(col_in[x],HIGH);

   pinMode(row[y],OUTPUT);

   //turn on the LED by driving the column low
   if(led_state[led]){
     digitalWrite(col_led[x],HIGH);
     delayMicroseconds(12);
     digitalWrite(col_led[x],LOW);
   }
   
   //Reset back to tri-state
   pinMode(row[y],INPUT);

   uint8_t button_read = digitalRead(row[y]);
   if(button_state[led] != button_read){
     if(button_read){
        button_pressed[led] = 1;
        handle_button(led,BUTTON_STATE_PRESS);
        last_repeat = millis();
     }else{
        button_released[led] = 1;
        handle_button(led,BUTTON_STATE_RELEASE);
     }
   }else{
     //is it a hold?  (State not changed, button is pressed)
     if(button_read){
       if(last_repeat+BUTTON_HOLD_REPEAT_MS < millis()){
         last_repeat=millis();
         //we should only send a hold repeat every ... milliseconds.
         //handle_button(led,BUTTON_STATE_HOLD);
       }
     }
   }

   
   button_state[led] = button_read;
   digitalWrite(col_in[x],LOW);
   
   led++;
   if(led ==(3*ROWCOUNT)){
    led=0;
   }

}

char * getSelectedAxis(){
  switch(group_get(GROUP_AXIS_SELECT)){
    case GROUP_AXIS_SELECT_A:
      return "A";
      break;
    default:
    case GROUP_AXIS_SELECT_X:
      return "X";
      break;
    case GROUP_AXIS_SELECT_Y:
      return "Y";
      break;
    case GROUP_AXIS_SELECT_Z:
      return "Z";
      break;
  }
       
}


uint32_t cmd_queue_waiting = 0;


#define WHEELMODE_JOG 0
#define WHEELMODE_JOG_PLAN 1
#define WHEELMODE_PLAY 2 //disabled
#define WHEELMODE_LAST 1

boolean wheelmode_change = 0;
uint32_t wheelmode = 0;

int32_t jog_amount = 0;

uint32_t wheelmode_timestamp = 0;

void send_value(int32_t val){
  Serial1.print(getSelectedAxis());
         
  if(val <0){
    Serial1.print("-");
    val=abs(val);
  }
  
  Serial1.print(val/1000);
  Serial1.print(".");
  val = val%1000;
  if(val <10){
    Serial1.print("00");
  }else if(val <100){
    Serial1.print("0");
  }
  Serial1.print(val);

}

void loop() {
  static long lastDisplayUpdate=0;
  static uint32_t unused_cycles = 0;

  static long lastStateQuery=0;

  
  if(digitalRead(PB0)==0 && wheelmode_change == 0){
    if(wheelmode == WHEELMODE_JOG_PLAN){
         Serial1.print("G91 G0 ");
         send_value(jog_amount);
         Serial1.println(" G90");
         Serial1.flush();
         cmd_queue_waiting++;
         jog_amount=0;
      
    }

    
    wheelmode++;
    if(wheelmode > WHEELMODE_LAST){
      wheelmode=0;
    }
    wheelmode_change = 1;


    
  }else if(digitalRead(PB0)==1 && wheelmode_change == 1){
    wheelmode_change = 0;
  }

  
  

  
  if (millis() - lastStateQuery >= 500) {
    Serial1.println("switch flood      ");
    Serial1.flush();
    lastStateQuery  = millis();
  }else if (millis() - lastDisplayUpdate >= 100) {
    display.clearDisplay();

    if(wheelmode == WHEELMODE_PLAY){
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.setTextSize(1);
      display.print("Play");
    }else{   
      //Status
      display.setCursor(0,0);
      if(strlen(statusMessage) > 20){
        display.setTextSize(1);  
      }else{
        display.setTextSize(2);
      }
    
      display.setTextColor(WHITE);
      display.println(statusMessage);




      //horizonal line dividing the top message, and bottom data sections.
      display.drawLine(0,32,128,32,WHITE);

  
      //feed display
      if(wheelmode == WHEELMODE_JOG_PLAN){
         display.setTextColor(WHITE);
         display.setCursor(0,32);
         display.setTextSize(1);
         
         display.setCursor(3,36);
         display.setTextSize(2);
         display.print(getSelectedAxis());
         int32_t val = jog_amount;
         
         if(val <0){
           display.print("-");
           val=abs(val);
         }
    
         display.print(val/1000);
         display.print(".");
         val = val%1000;
         if(val <10){
            display.print("00");
         }else if(val <100){
            display.print("0");
         }
         display.print(val);
         
      }else{
        display.setCursor(48,36);
        display.setTextSize(3);
        display.setTextColor(WHITE);
        display.print("Jog");
      }
    }
    //queue length
    
    if(cmd_queue_waiting >0){
      if(cmd_queue_waiting > 10){
        display.setCursor(0,42);
        display.setTextSize(3);
      }else if(cmd_queue_waiting > 5){
        display.setCursor(0,48);
        display.setTextSize(2);
      }else{
        display.setCursor(0,56);
        display.setTextSize(1);
      }
      display.setTextColor(WHITE);
      display.print(cmd_queue_waiting,DEC);
    }

   
    display.display();
    lastDisplayUpdate = millis();
    
    unused_cycles=0;
  }else if(Serial1.available()){
    inbuffer[inbuffer_idx] = Serial1.read();
    if(inbuffer[inbuffer_idx] == '\n'){
      inbuffer[inbuffer_idx]=0;
      
      if(strncmp("ALARM:",inbuffer,6)==0){
        led_state[ALARM_LED] =1;
         strncpy(statusMessage,inbuffer+6,sizeof(statusMessage)-1);
      }else if(strncmp("ok",inbuffer,2)==0){
        if(cmd_queue_waiting > 0){
          cmd_queue_waiting--;
        }
      }else if(strncmp("switch flood is",inbuffer,13)==0){
        if(inbuffer[14] == '0'){
          led_state[PANEL1_SW1] = 0;
        }else if(inbuffer[14] == '1'){
          led_state[PANEL1_SW1] = 1;
        }
      //TODO: UnKill button pressed Halt cleared
      }else{
        if(strlen(inbuffer)>0){
          strncpy(statusMessage,inbuffer,sizeof(statusMessage)-1);
        }
        memset(inbuffer,0,sizeof(inbuffer));
      }
      inbuffer_idx=0;
    }else{
      inbuffer_idx++;
    }
    
  }else if (millis() - interval >= 10) {
     
     if(timer.getCount() != 500){
       int val = timer.getCount()-500;
       timer.setCount(500);

       
       int selected = group_get(GROUP_D_PER_PULSE);
    
       switch(selected){
         case GROUP_D_PER_PULSE_0500:
           val*=500; //0.5 mm
           break;
         case GROUP_D_PER_PULSE_0100:
           val*=100; //0.1 mm
           break;
         case GROUP_D_PER_PULSE_0010:
           val*=10; //0.01 mm
           break;
         default:
         case GROUP_D_PER_PULSE_0005:
           val*=5; //0.005 mm
           break;
       }
 
       if(wheelmode == WHEELMODE_JOG_PLAN){
         jog_amount+= val;
       }else if(wheelmode == WHEELMODE_JOG){
         
         
         Serial1.print("G91 G0 ");
         send_value(val);
         Serial1.println(" G90");
         Serial1.flush();
         cmd_queue_waiting++;

       }
       
       interval=millis();
    }
  }else{
     unused_cycles++;
  }
}


bool button_toggle(int id){
  if(led_state[id]){
    led_state[id] = 0;
  }else{
    led_state[id] = 1;
  }
  return led_state[id];
}



void group_init(int count,...){
  va_list ap;
  
  va_start(ap, count);
  led_state[va_arg(ap, int)] = 1;
  va_end(ap);
}

/** Gets the currently active switch from a group
 *  
 */
int group_get(int count,...){
  va_list ap;
  int j;
  
  int retval=-1;
  
  va_start(ap, count);
  for (j = 0; j < count; j++) {
    int i = va_arg(ap, int);
    if(led_state[i]){
      retval = i;
    }
  }
  va_end(ap);
  return retval;
}





void group_toggle(int on_id,int count,...){
  va_list ap;
  int j;
  
  va_start(ap, count);
  for (j = 0; j < count; j++) {
    led_state[va_arg(ap, int)] = 0;
  }
  va_end(ap);
  if(on_id >=0){
    led_state[on_id] = 1;
  }
}



void handle_button(int id,uint8_t state){
  switch(id){
    case PANEL1_SW1: //Flood
      if(state == BUTTON_STATE_PRESS){
        if(button_toggle(id)){
          Serial1.println("M8");
          cmd_queue_waiting++;
        }else{
          Serial1.println("M9");
          cmd_queue_waiting++;
        }
      }
      break;

    case PANEL1_SW2: //Light
      if(state == BUTTON_STATE_PRESS){
        if(button_toggle(id)){
          Serial1.println("M355");
          cmd_queue_waiting++;
        }else{
          Serial1.println("M356");
          cmd_queue_waiting++;
        }  
      }
      break;

    case PANEL1_SW3: //Air
      if(state == BUTTON_STATE_PRESS){
        if(button_toggle(id)){
          Serial1.println("M7");
          cmd_queue_waiting++;
        }else{
          Serial1.println("M9");
          cmd_queue_waiting++;
        }  
      }
      break;


   default:
      if(state == BUTTON_STATE_PRESS){
        if(button_toggle(id)){
          ;
        }  
      }
      break;


   case PANEL2_SW9: //Plus jog
   case PANEL2_SW14: //Minus jog
      if(state == BUTTON_STATE_PRESS || state == BUTTON_STATE_HOLD){
       int val = 1;
       if(id == PANEL2_SW14){
         val=-1;
       }
       int selected = group_get(GROUP_D_PER_PULSE);
    
       switch(selected){
         case GROUP_D_PER_PULSE_0500:
           val*=500; //0.5 mm
           break;
         case GROUP_D_PER_PULSE_0100:
           val*=100; //0.1 mm
           break;
         case GROUP_D_PER_PULSE_0010:
           val*=10; //0.01 mm
           break;
         default:
         case GROUP_D_PER_PULSE_0005:
           val*=5; //0.005 mm
           break;
       }
       //jog_amount+= val;
       
       Serial1.print("G91 G0 ");
       send_value(val);
       Serial1.print(" G90");
       cmd_queue_waiting++;
     }
     break;
     
   case PANEL2_SW15: //Home
      if(state == BUTTON_STATE_PRESS){
        Serial1.print("G28.2 ");
        Serial1.print(getSelectedAxis());
        Serial1.println("0");
        cmd_queue_waiting++;
      }
      break;
      

    case PANEL2_SW1: //Feed 75%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S75");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    case PANEL2_SW2: //Feed 100%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S100");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    case PANEL2_SW3: //Feed 125%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S125");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    case PANEL2_SW4: //Feed 150%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S150");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    case PANEL2_SW6: //Feed 50%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S50");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    case PANEL2_SW11: //Feed 25%
      if(state == BUTTON_STATE_PRESS){
        Serial1.println("M220 S25");
        cmd_queue_waiting++;
        group_toggle(id,GROUP_FEED_OVERRIDE);
      }
      break;

    
    case PANEL2_SW7: //Move Forward 6.35mm (1/4 inch)
      if(state == BUTTON_STATE_PRESS){
       Serial1.print("G91 G0 ");
       Serial1.print(getSelectedAxis());
       Serial1.println("6.35 G90");
       cmd_queue_waiting++;
      }
      break;

    case PANEL2_SW12: //Move Backward 6.35mm (1/4 inch)
      if(state == BUTTON_STATE_PRESS){
       Serial1.print("G91 G0 ");
       Serial1.print(getSelectedAxis());
       Serial1.println("-6.35 G90");
       cmd_queue_waiting++;
      }
      break;

    case PANEL2_SW8: //Move Forward 0.1 inch (1/10 inch)
      if(state == BUTTON_STATE_PRESS){
       Serial1.print("G91 G0 ");
       Serial1.print(getSelectedAxis());
       Serial1.println("2.54 G90");
       cmd_queue_waiting++;
      }
      break;

    case PANEL2_SW13: //Move Backward 0.1 inch (1/10 inch)
      if(state == BUTTON_STATE_PRESS){
       Serial1.print("G91 G0 ");
       Serial1.print(getSelectedAxis());
       Serial1.println("-2.54 G90");
       cmd_queue_waiting++;
      }
      break;

    case PANEL2_SW10: //Motors off
      if(state == BUTTON_STATE_PRESS){
       Serial1.println("M84");
       cmd_queue_waiting++;
      }
      break;

    
    case PANEL2_SW5: //Clear Alarm
      if(state == BUTTON_STATE_PRESS){
        if(led_state[ALARM_LED]) {//is alarm set?
          Serial1.println("M999");
          strncpy(statusMessage,"Cleared",sizeof(statusMessage)-1);
          cmd_queue_waiting++;
        }
      }else{ //release
        led_state[ALARM_LED] =0;
      }
      break;
      
    case PANEL1_SW10: //Go Origin
      if(state == BUTTON_STATE_PRESS){
        Serial1.print("G53 G0 ");
        Serial1.print(getSelectedAxis());
        Serial1.println("0");
        cmd_queue_waiting++;
      }
      break;

    case PANEL1_SW5: //Go WCS Zero
      if(state == BUTTON_STATE_PRESS){
        Serial1.print("G0 ");
        Serial1.print(getSelectedAxis());
        Serial1.println("0");
        cmd_queue_waiting++;
      }
      break;

    case PANEL1_SW15: //Set G92 zero position
      if(state == BUTTON_STATE_PRESS){
        Serial1.print("G92 ");
        Serial1.print(getSelectedAxis());
        Serial1.println("0");
        cmd_queue_waiting++;
      }
      break;


    case PANEL1_SW11:
    case PANEL1_SW12:
    case PANEL1_SW13:
    case PANEL1_SW14:
      if(state == BUTTON_STATE_PRESS){
        group_toggle(id,GROUP_D_PER_PULSE);
      }
      break;

    case PANEL1_SW6:
    case PANEL1_SW7:
    case PANEL1_SW8:
    case PANEL1_SW9:
      if(state == BUTTON_STATE_PRESS){
        group_toggle(id,GROUP_AXIS_SELECT);
      }
      break;


      
  }

}

