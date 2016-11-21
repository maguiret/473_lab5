#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include "hd44780.h"
#include "lcd_functions.h"
#include "lm73_functions_skel.h"
#include "twi_master.h"

/*******************
 for internal sensor
********************/
char    lcd_string_array[16];  //holds a string to refresh the LCD
//uint8_t i;                     //general purpose index

//delclare the 2 byte TWI read and write buffers (lm73_functions_skel.c)
extern uint8_t lm73_wr_buf[2];
extern uint8_t lm73_rd_buf[2];

/*************
for lcd display
**************/
//#define CMD_BYTE 0x01
//char on[9] = "ALARM ON";
//char off[9]= "ALARM OFF";
uint8_t i = 0;
uint8_t dimFlag = 0x00;
uint8_t a_current = 0, b_current = 0, a_past = 0, b_past = 0;
/*************
for adc
**************/
uint8_t  i;              //dummy variable
uint16_t adc_result;     //holds adc result 
char     lcd_str_h[16];  //holds string to send to lcd  
char     lcd_str_l[16];  //holds string to send to lcd  
div_t    fp_adc_result, fp_low_result;  //double fp_adc_result; 

/******
for ISR
*******/
#define en1A 0
#define en1B 1
#define en2A 2
#define en2B 3
uint8_t encoder = 0;
uint8_t prevEncoder0 = 1;
uint8_t prevEncoder1 = 1;

/************************
  for brightness of 7-seg
*************************/
#define bright 0xAA		// off=00    on =55

/************************
 for alarm 
*************************/
#define freq 0xC5F0//10096		// between 62000 and 10096
#define volume 0xC5F0		// 0xFFF0 = off
int aHour = 0, aMinute = 1, sHour = 0, sMinute = 1;
//uint8_t aHour 	= 0;
//uint8_t aMinute = 1;
//uint8_t sHour 	= 5;
//uint8_t sMinute = 1;
uint8_t alarming = 0;
uint8_t snoozeFlag = 0;

/********************************
Modes the Alarm Clock will be in 
********************************/
enum modes{clk, setClk, setAlarm};

/*************
global variables
***************/
uint8_t count_7ms = 0;

/*************
for the 7-seg
*************/
volatile int16_t switch_count = 0;
int minute = 0, hour = 0, minOne = 0, minTen = 0, hOne = 0, hTen = 0;
int alarmMinute = 0, alarmHour = 0;
uint8_t colon = 0xFF, one = 0, ten = 0;

/************************************************************************
Just like bit_is_clear() but made it check a variable
************************************************************************/
uint8_t var_bit_is_clr(uint8_t test_var, uint8_t bit) {
 if((test_var & (1 << bit))  == 0){
  return  1;//TRUE;
 }
 else{
  return 0;//FALSE;
 }
}//var_bit_is_clr

/************************************************************************
Checks the state of the button number passed to it. It shifts in ones till
the button is pushed. Function returns a 1 only once per debounced button
push so a debounce and toggle function can be implemented at the same time.
Adapted to check all buttons from Ganssel's "Guide to Debouncing"
Expects active low pushbuttons on a port variable.  Debounce time is
determined by external loop delay times 12.
Note: that this function is checking a variable, not a port.
************************************************************************/
uint8_t debounceSwitch(uint8_t button_var, uint8_t button) {
 static uint16_t state[8]= {0,0,0,0,0,0,0,0};  					// holds shifted in bits from buttons
 state[button] = (state[button] << 1) | (! var_bit_is_clr(button_var, button)) | 0xE000;
 if (state[button] == 0xF000){
  return 1; 									// if held true for 12 passes through
 }
 else{
  return 0;
 }
} //debounceSwitch

/***************************************
 converts decimal value to 7-seg display
***************************************/
void LEDSegment(int x){ 
 if(x == 0){
 PORTA = 0xC0;
 }
 if(x == 1){
  PORTA = 0xF9; 
 } 
 if(x == 2){
  PORTA = 0xA4; 
 } 
 if(x == 3){
  PORTA = 0xB0; 
 } 
 if(x == 4){
  PORTA = 0x99; 
 } 
 if(x == 5){
  PORTA = 0x92; 
 } 
 if(x == 6){
  PORTA = 0x82; 
 } 
 if(x == 7){
  PORTA = 0xF8; 
 } 
 if(x == 8){
  PORTA = 0x80; 
 }
 if(x == 9){
  PORTA = 0x98;
 }
 if(x > 9){    			// error indicator
  PORTA = 0x00;
 }
}
/***************************************************************
 saves the ones, tens, hundreds, thousands place of switch_count
***************************************************************/
uint8_t position0(uint16_t x){
  int value = x;
  one = value %10;
  return one;
//  value -= one;
//  
//  ten = (value %100)/10;
//  value -= ten;
//  
//  hundred = (value %1000)/100;
//  value -= hundred;
// 
//  thousand = (value %10000)/1000;
}
uint8_t position1(uint16_t x){
  int value = x;
  one = value %10;
  value -= one;
  
  ten = (value %100)/10;
  return ten;
}
/***************************************************
 checks encoders and changes hour/minute accordinly
****************************************************/
void buttonSense(){
  if(((encoder & (1<<en1A)) == 0) & (prevEncoder0 == 1)){ // checks for falling edge of
                                                         // encoder1.A 
    if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  // if encoder1.B is different from encoder1.A
      //increase;                                    // encoder1 is turning clockwise
    }
    else{
      //decrease
    }
  }
  prevEncoder0 = (encoder & (1<<en1A));
}

void segButtonOutputSet(){
  DDRA = 0xFF;                           // segments on/pushbuttons off
  PORTA = 0xFF;                          // set segments to default off
  _delay_ms(1);
}
void segButtonInputSet(){
 PORTF = 0x70;                          // tristate buffer for pushbuttons enabled
 DDRA = 0x00;                           // PORTA set as input
 PORTA = 0xFF;                          // PORTA as pullups
 _delay_ms(1);
}

void segButtonInit(){
  DDRA = 0xFF; 		 		 // segments on/pushbuttons off
  DDRF |= 0x70;		 		 // pin 4-7 on portb set to output
  PORTF = 0x00;		 		 // digit 4. one = 0x00 ten = 0x10 
			 		 //hundred = 0x30 thousand = 0x40
}

/******************************
 initialize clock
*******************************/
void tcnt0_init(void){
  ASSR  |=  (1<<AS0);                //run off external 32khz osc (TOSC)
  TIMSK |= (1<<OCIE0);		     //enable interrupts for output compare match 0
  TCCR0 |=  (1<<WGM01) | (1<<CS00);  //CTC mode, no prescale
  OCR0  |=  0x07f;                   //compare at 128
}

/******************************
 initialize dimming 
*******************************/
void tcnt2_init(void){ //pg. 159
  // fast PWM, no prescale, inverting mode
  TCCR2 |= (1<<WGM21)|(1<<WGM20)|(1<<COM21)|(1<<COM20)|(1<<CS0);
//  TCCR2 =  (1<<WGM21)|(1<<WGM20)|(1<<COM21)|(1<<COM20)|(1<<CS20)|(1<<CS21);
  OCR2 = bright;		//compare @ 123 PB7
}

/******************************
 initialize alarm noise 
*******************************/
// CTC TOP = ICR1, no prescale, invert
void tcnt1_init(void){
  TCCR1A |= (1<<COM1A0);
//  TCCR3B |= (1<<WGM33)|(1<<WGM32)|(1<<CS31)|(1<<CS30);
  TCCR1B |= (1<<WGM12)|(1<<CS10);
  TCCR1C = 0x00;
  TCNT1  = 0;
  OCR1A  = freq; //PE3
//  ICR1   = 0xFFFF;

//  TCCR1A |= (1<<COM1A0);
//  TCCR1B |= (1<<WGM12)|(1<<CS10);
//  TCCR1C = 0x00;
//  TCNT1  = 0;
//  OCR1A  = freq; //PB5  
}
/******************************
 initialize alarm volume 
*******************************/

void tcnt3_init(void){//2.3 Vrms
 //inverting, fast PWM TOP ICR3, 64 prescaler
  TCCR3A |= (1<<COM3A1)|(1<<COM3A0)|(1<<WGM31);
//  TCCR3B |= (1<<WGM33)|(1<<WGM32)|(1<<CS31)|(1<<CS30);
//  TCCR3B |= (1<<WGM33)|(1<<WGM32)|(1<<CS30);
  TCCR3B |= (1<<WGM33)|(1<<WGM32);
  TCCR3C = 0x00;
  TCNT3  = 0;
  OCR3A  = volume; //PE3
  ICR3   = 0xFFFF;
}

/********************************************************************** 
collects the tens and ones place of hour and minutes based on count_7ms
When the TCNT0 overflow interrupt occurs, the count_7ms variable is    
incremented.  Every 7680 interrupts the minutes counter is incremented.
tcnt0 interrupts come at 7.8125ms internals.
 1/32768         = 30.517578uS
(1/32768)*256    = 7.8125ms
(1/32768)*256*64 = 500mS 
*i*********************************************************************/ 
void timeExtract(){
//  if((count_7ms %128) == 0){ 		// if one second has passed
  if((count_7ms %16) == 0){ 		// for debugging. REMOVE on final
    switch_count++;
    colon ^= 0xFF;			// toggling the colon every second
  }
  if(switch_count == 60){		// if 60 seconds have passed
    minute++;
    switch_count = 0;
    if(minute == 60){
      hour++;
      minute = 0;
      if(hour == 24){
        hour = 0;
      }
    }
  }
  
}

/*************************************************************************
                           timer/counter0 ISR                          
 When the TCNT0 compare interrupt occurs, the count_7ms variable is    
 incremented. 
  1/32768         = 30.517578uS
 (1/32768)*128    = 3.90625ms
 (1/32768)*256*128 = 1000mS
*************************************************************************/
ISR(TIMER0_COMP_vect){
  count_7ms++;
  // determing time positions
  timeExtract();
}


/******************************************************
 This function displays each digit's value on the 7-seg
*******************************************************/
void segmentDisplay(uint8_t flag){
  // this section handles the 7-seg displaying segments
    PORTF &= (0<<PF6)|(0<<PF5)|(0<<PF4);//0x00;		// setting digit position 
    LEDSegment(minOne);					// settings segments based on digit position
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 		// eliminates all ghosting
  
  // displaying tens
    PORTF = (0<<PF6)|(0<<PF5)|(1<<PF4);//0x10;
      LEDSegment(minTen);
    _delay_us(300);					
    PORTA = 0xFF;
  			
  // displaying colon
    PORTF =(0<<PF6)|(1<<PF5)|(0<<PF4);// 0x30;
    if(colon){
      PORTA = 0xFC; 
    }
    else{
      PORTA = 0xFF;
    }
    _delay_us(300);				
    PORTA = 0xFF;			 
  
  //displaying hundreds
    PORTF =(0<<PF6)|(1<<PF5)|(1<<PF4);// 0x30;
//    if(hour <10){
//      PORTA = 0xFF;
//    }
//    else{
      LEDSegment(hOne);
//    }
    _delay_us(300);			
    PORTA = 0xFF;			 
  
    PORTF =(1<<PF6)|(0<<PF5)|(0<<PF4);// 0x40;
    if(flag == 1){
      if(hour<10){
        PORTA = 0xFF;
      }
      else{
        LEDSegment(hTen);
      }
    }
    else if(flag == 2){
      if(aHour<10){
        PORTA = 0xFF;
      }
      else{
        LEDSegment(hTen);
      }
    }
    _delay_us(300);		
    PORTA = 0xFF;			 	
}

/*******************************
Initialize Spi
********************************/
void spi_init(void){
  DDRB  |=   (0x07)|(1<<PB7)|(1<<PB5);//Turn on SS, MOSI, SCLK, pwm for 7-seg, volume for alarm
  PORTB |= _BV(PB1);
  DDRF  |= 0x08;
  PORTF &= 0xF7;

  SPCR  |=   (1<<SPE)|(1<<MSTR);//set up SPI mode
  SPSR  |=   (1<<SPI2X);// double speed operation
 }//spi_init

void encoder_init(){
  DDRE |= (1<<PE5)|(1<<PE6)|(1<<PE3);
} 

void adc_get(){
 ADCSRA |=(1<<ADSC);

 while(bit_is_clear(ADCSRA, ADIF)){};//spin while interrupt flag not set

 ADCSRA |= (1<<ADIF);//its done, clear flag by writing a one 

  adc_result = ADC;                      //read the ADC output as 16 bits

  //div() function computes the value num/denom and returns the quotient and
  //remainder in a structure called div_t that contains two members, quot and rem. 

  //now determine Vin, where Vin = (adc_result/204.8)
  fp_adc_result = div(adc_result, 205);              //do division by 205 (204.8 to be exact)
  if(fp_adc_result.quot >= 3){
//  if((adc_result/205) >= 2){ //room = <.8, dark = 3, light = 
    OCR2 = 0x0f;
  }
  else if(fp_adc_result.quot >=2){
    OCR2 = 0x20;
  }
  else{
    OCR2 = 0xF0;
  }

}
/***************************************************************
collects the encoder input and changes hour, minutes accordinly
used for LCD too
****************************************************************/
void encoderInput(uint8_t flag){
  // loading encoder data into shift register
  PORTE |= (1<<PE5);             				// sets CLK INH high
  PORTE &= ~(1<<PE6);            				// toggle SHLD low to high           
  PORTE |= (1<<PE6);             
  //shifting data out to MISO
  PORTE &= ~(1<<PE5);
  SPDR = 0x00;							// garbage data
  while(bit_is_clear(SPSR,SPIF)) {}              		// wait till data sent out 
  encoder = SPDR;                                  		// collecting input from encoders
  PORTE |= (1<<PE5);                               		// setting CLK INH back to high
  if(flag == 2){ 						// setting time
    if(((encoder & (1<<en1A)) == 0) && (prevEncoder0 == 1)){ 	// checks for falling edge of
                                                           	// encoder1.A 
      if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  	// if encoder1.B is different from encoder1.A, clockwise
        hour++;
      }
      else{
        hour--;
      }
    }
    prevEncoder0 = (encoder & (1<<en1A));
// second encoder check why doesn't my logic work for this too?
    a_current = ((encoder>>2) & 0x01);
    b_current = ((encoder>>3) & 0x01);
  
    if(a_past == a_current){
      if((a_current==1) && (b_past < b_current)){
        minute++;
    }
    if((a_current == 1) && (b_past > b_current)){
      minute--;
    }
  //    if((a_current == 0) && (b_past > b_current)){
  //      minute++;
  //    }
  //    if((a_current == 0) && (b_past < b_current)){
  //      minute--;
  //    }
    } 
    a_past = a_current;
    b_past = b_current;
  }
//sets the alarm hour
  else if(flag == 1){ 						// setting alarm clock
    if(((encoder & (1<<en1A)) == 0) && (prevEncoder0 == 1)){ 	// checks for falling edge of
                                                           	// encoder1.A 
      if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  	// if encoder1.B is different from encoder1.A, clockwise
        aHour++;
      }
      else{
        aHour--;
      }
    }
  prevEncoder0 = (encoder & (1<<en1A));
// second encoder check why doesn't my logic work for this too?
   a_current = ((encoder>>2) & 0x01);
   b_current = ((encoder>>3) & 0x01);
  
  if(a_past == a_current){
      if((a_current==1) && (b_past < b_current)){
        aMinute++;
      }
      if((a_current == 1) && (b_past > b_current)){
        aMinute--;
      }
  //    if((a_current == 0) && (b_past > b_current)){
  //      minute++;
  //    }
  //    if((a_current == 0) && (b_past < b_current)){
  //      minute--;
  //    }
    } 
    a_past = a_current;
    b_past = b_current;
  }
//sets alarm clock +5 minutes (snooze)
//  else if(flag == 0){						// setting snooze
//    if(((encoder & (1<<en1A)) == 0) && (prevEncoder0 == 1)){ 	// checks for falling edge of
//                                                               	// encoder1.A 
//      if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  	// if encoder1.B is different from encoder1.A, clockwise
//        hour++;
//      }
//      else{
//        hour--;
//      }
//    }
//    prevEncoder0 = (encoder & (1<<en1A));
//    // second encoder check why doesn't my logic work for this too?
//    a_current = ((encoder>>2) & 0x01);
//    b_current = ((encoder>>3) & 0x01);
//      
//    if(a_past == a_current){
//      if((a_current==1) && (b_past < b_current)){
//        minute++;
//      }
//      if((a_current == 1) && (b_past > b_current)){
//         minute--;
//      }
//   //    if((a_current == 0) && (b_past > b_current)){
//   //      minute++;
//   //    }
//   //    if((a_current == 0) && (b_past < b_current)){
//   //      minute--;
//   //    }
//    } 
//    a_past = a_current;
//    b_past = b_current;
//  }

}

/**********************************************
 initalize the LCD
 Take from R. Traylor 11.14.2016 
***********************************************/
void lcdInit(void){
        _delay_ms(15);
       // request 8 bit interface mode
        SPDR = 0x00;
        while (!(SPSR & 0x80)) {}
        SPDR = 0x38;
        while (!(SPSR & 0x80)) {}
        PORTF |= 0x08;
        PORTF &= ~0x08;
        _delay_us(100);


//      LCD_CMD(0x38);
        _delay_ms(5);

        // display off
        SPDR = 0x00;
        while (!(SPSR & 0x80)) {}
        SPDR = 0x08;
        while (!(SPSR & 0x80)) {}
        PORTF |= 0x08;
        PORTF &= ~0x08;
        _delay_us(100);
        //LCD_CMD(0x08);
        _delay_ms(2);

        // choose entry mode so that the cursor is incremented
        SPDR = 0x00;
        while (!(SPSR & 0x80)) {}
        SPDR = 0x06;
        while (!(SPSR & 0x80)) {}
        PORTF |= 0x08;
        PORTF &= ~0x08;
        _delay_us(100);
        //LCD_CMD(0x06);
        /*
                Clear the screen and enable the LCD
        */
        // clear display
        SPDR = 0x00;
        while (!(SPSR & 0x80)) {}
        SPDR = 0x01;
        while (!(SPSR & 0x80)) {}
        PORTF |= 0x08;
        PORTF &= ~0x08;
        _delay_us(100);
//      LCD_CMD(0x01);
        _delay_ms(5);

        // display on
        SPDR = 0x00;
        while (!(SPSR & 0x80)) {}
        SPDR = 0x0C;
        while (!(SPSR & 0x80)) {}
        PORTF |= 0x08;
        PORTF &= ~0x08;
	_delay_us(100);
}

/**********************************************
 initalize the LCD
 Take from R. Traylor 11.14.2016 
***********************************************/
void lcdPutStr(char *lcd_str) {
  uint8_t count;
  for (count=0; count<=(strlen(lcd_str)-1); count++){
    //LCD_DATA(lcd_str[count]);
    SPDR = 0x01;                            
    while (!(SPSR & 0x80)) {}       
    SPDR = lcd_str[count];                                     
    while (!(SPSR & 0x80)) {}       
    PORTF |= 0x08;                          
    PORTF &= ~0x08;                         
    _delay_us(100);                         
  }
}


int main(){
  // initialize
segButtonInit();					// (must be in, why?)initialize the
							//  external pushButtons and 7-seg
  // initializing adc
  DDRF  &= ~(_BV(DDF7));  
  PORTF &= ~(_BV(PF7)); 
  ADMUX |= (1<<REFS0)|(1<<MUX2)|(1<<MUX1)|(1<<MUX0);
  ADCSRA |= (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
  
  tcnt3_init();						// alarm volume
  tcnt2_init();						// dimming
  tcnt1_init();						// alarm noise
  tcnt0_init();						// initialize clock
  spi_init();
  lcdInit();
  encoder_init();
  init_twi();
  uint16_t lm73_temp;
  lm73_wr_buf[0] = 0x00;
  twi_start_wr(0x90, lm73_wr_buf[0], 1);
  sei();						// enable interrupts before entering loop
  clear_display();
  //set default mode
  enum modes mode = clk;  

  while(1){
    // implements mode
    switch(mode){
      case clk:{
        segButtonOutputSet();
        // saving the hour and minute digits
        minOne = position0(minute);               	 
        minTen = position1(minute);
        hOne = position0(hour);
        hTen = position1(hour);
        segButtonOutputSet();				// switches from push buttons to display
    // adc part
        if(dimFlag == 100){				//dimmer functionality
          adc_get();
          dimFlag = 0;
        }
// alarm sounding check
        if(alarming == 1){
          if((minute == aMinute) && hour == aHour){
            TCCR3B |=(1<<CS30);				// turning alarm on
          }
          segButtonInputSet();
          if(debounceSwitch(PINA,6)){
            TCCR3B &= ~(1<<CS30);				// turning alarm off
            OCR3A = 0xFFF0;
            alarming = 0;					//turning alarming flag off
          }
          else if(debounceSwitch(PINA, 7)){
            OCR3A = 0xFFF0;
            TCCR3B &= ~(1<<CS30);				// turning snooze on
            sMinute = minute + 1;
            snoozeFlag = 1;
            if(sMinute >= 60){
              sHour++;
              sMinute = sMinute - 60;
            }
          }
          if(snoozeFlag == 1){					// alarm on when snooze time is visited on clk
            if((minute == sMinute) && (hour == sHour)){
              TCCR3B |=(1<<CS30);
              snoozeFlag = 0;
            }
          }
          lcdPutStr("Alarm is On ");				//putting string on lcd
//resetting cursor to start
          SPDR = 0x00;                            
          while (!(SPSR & 0x80)) {}       
          SPDR = 0x02;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);                         
        }
        else{
          lcdPutStr("Clock Mode ");				//putting string on lcd
//resetting cursor to start
          SPDR = 0x00;                            
          while (!(SPSR & 0x80)) {}       
          SPDR = 0x02;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);                         
        }
//displaying inside temperature
///////////////////////////////////////////////////////
          SPDR = 0x00;                            //setting cursor at second column
          while (!(SPSR & 0x80)) {}       
          SPDR = 0xC0;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);              
          
          twi_start_rd(0x90, lm73_rd_buf, 2);
          _delay_ms(2);
          lm73_temp = lm73_rd_buf[0]; //save high temperature byte into lm73_temp
          lm73_temp = (lm73_temp << 8); //shift it into upper byte 
          lm73_temp |= lm73_rd_buf[1]; //"OR" in the low temp byte to lm73_temp 
          uint16_t temp = (lm73_temp >> 7);
          itoa(temp, lcd_string_array, 10); //convert to string in array with itoa() from avr-libc   
          lcdPutStr(lcd_string_array); 

//resetting cursor to start
          SPDR = 0x00;
          while (!(SPSR & 0x80)) {}
          SPDR = 0x02;
          while (!(SPSR & 0x80)) {}
          PORTF |= 0x08;
          PORTF &= ~0x08;
          _delay_ms(2);
 
/////////////////////////////////////////////////////          

//}
//lcd part
//        lcd_init();
//        refresh_lcd(on);
        segButtonOutputSet();
        segmentDisplay(1);				// displaying the 7-seg
        dimFlag++;      
        OCR3A = 0xC5F0;
        break;
      }
      case setClk:{
        segButtonInputSet();
        while(!(debounceSwitch(PINA, 0))){ 		// user confirmation may change to button 7
	  encoderInput(2);				// checks encoder states and handles min/hour
// taking care of limits
          if(minute == 60){
            hour++;
            minute = 0;
          }
          else if (minute == -1){
           hour--;
           minute = 59; 
          }
          if(hour == 24){
              hour = 0;
          }
          else if(hour == -1){
            hour = 23;
          }
          minOne = position0(minute);               	 
          minTen = position1(minute);
          hOne = position0(hour);
          hTen = position1(hour);
          segButtonOutputSet();				// switches from push buttons to display
          segmentDisplay(1);				// displaying the 7-seg
          segButtonInputSet();

          lcdPutStr("What time is it? ");				//putting string on lcd
//resetting cursor to start
          SPDR = 0x00;                            
          while (!(SPSR & 0x80)) {}       
          SPDR = 0x02;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);                         
        }
        segButtonOutputSet();
        mode = clk;        

          SPDR = 0x00;                            
          while (!(SPSR & 0x80)) {}       
          SPDR = 0x01;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);                         
        break;
      }
      case setAlarm:{
        segButtonInputSet();
        while(!(debounceSwitch(PINA, 1))){ 		// user confirmation may change to button 7
	  encoderInput(1);				// checks encoder states and handles min/hour
// taking care of limits
          if(aMinute == 60){
            aHour++;
            aMinute = 0;
          }
          else if (aMinute == -1){
           aHour--;
           aMinute = 59; 
          }
          if(aHour == 24){
              aHour = 0;
          }
          else if(aHour == -1){
            aHour = 23;
          }
          minOne = position0(aMinute);               	 
          minTen = position1(aMinute);
          hOne = position0(aHour);
          hTen = position1(aHour);
          segButtonOutputSet();				// switches from push buttons to display
          segmentDisplay(2);				// displaying the 7-seg
          segButtonInputSet(); 

          lcdPutStr("What time do you");				//putting string on lcd
//resetting cursor to start
          SPDR = 0x00;                            //setting cursor at second column
          while (!(SPSR & 0x80)) {}       
          SPDR = 0xC0;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);              
           
	  lcdPutStr("want to wake up?");

          SPDR = 0x00;                            
          while (!(SPSR & 0x80)) {}       
          SPDR = 0x02;                                     
          while (!(SPSR & 0x80)) {}       
          PORTF |= 0x08;                          
          PORTF &= ~0x08;                         
          _delay_ms(2);                         
        } 
        segButtonOutputSet();
        mode = clk;
        alarming = 1;

        SPDR = 0x00;                            
        while (!(SPSR & 0x80)) {}       
        SPDR = 0x01;                                     
        while (!(SPSR & 0x80)) {}       
        PORTF |= 0x08;                          
        PORTF &= ~0x08;                         
        _delay_ms(2);                         
        break;
      }
    }
    //  checks buttons
    segButtonInputSet();
    if(debounceSwitch(PINA,0)){
      mode = setClk;
    }
    else if(debounceSwitch(PINA,1)){
      mode = setAlarm;
    }
    segButtonOutputSet();
  }//while

}//main
