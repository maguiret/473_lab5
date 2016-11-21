/*
 * ECE473_SineWaveGenerator.c
 *	Fall2016
 * Created: 11/6/2016 2:00:23 PM
 * Author : Justin Martinez
 */ 
//******************************************************************************************************
//********************	 Skeletoncode:	 ***************************************************************
//******************************************************************************************************
// siren.c  - R. Traylor  - 10.31.2013
//
// Setup TCNT1 to be a varaible frequency tone generator. Its made variable by dynamically
// changing the compare register.
//
// Setup TCNT3 to interrupt the processor at a rate of 1000 times a second.  When the
// interrupt occurs, the ISR for TCNTR3 changes the frequency of timer TCNT1 to affect
// a continually changing audio frequency at PORTB bit 5.
//
// set OC1A (PB5) as the audio output signal PORTB (PB7) is the PWM signal for creating
// the volume control.
//
// Timer TCNT3 is set to interrupt the processor at a rate of 1000 times
// a second.  When the interrupt occurs, the ISR for TCNTR3 changes the
// frequency of timer TCNT1 to affect a continually changing audio
// frequency at PORTB bit 5.
//
// to download:
// wget http://www.ece.orst.edu/~traylor/ece473/inclass_exercises/timers_and_counters/siren_skeleton.c

#include <avr/io.h>
#include <avr/interrupt.h>
void _initalize(void);
ISR(TIMER3_COMPA_vect) {
	//OCR1A values that work well are from 10000 to 65000
	//the values should increment and decrement by about 64
	//
	static uint16_t count=0;
	static uint8_t up=1;
	if(up==1){
		if(count < 62000){ count = count + 64;}
		else {count = count - 64; up=0;}
	}//if
	if(up==0){
		if(count > 10096){ count = count - 64 ;}
		else{count = count + 64; up= 1;}
	}//if
	OCR1A = count;
}//ISR
//**************************************************************************************************************************K
//***********	Main	****************************************************************************************************
//**************************************************************************************************************************
int main(){
	_initalize();
//	sei();     //set GIE to enable interrupts
        OCR1A = 10000;
	while(1){
	} //do forever
}  // main

//**************************************************************************************************************************
//***********	Functions	************************************************************************************************
//**************************************************************************************************************************
void _initalize(void){
	// Set Up PORTS:
	DDRB   |= (1<<PB5);                          //set port B bit five and seven as outputs

	// Set Timer/Counter1:  CTC mode, OCR1A == Top, Update == immediate, Flag Set On == Top
	// waveform generation mode #4, with OCR1A On, No pre-sacling, Toggle output pin
	TCCR1A |=(1<<COM1A0);					// Compare variable OCR1A turned On for toggle PB7, Wave form generation: WGM11 = 0 & WGM12 = 0
	TCCR1B |=(1<<WGM12)| (1<<CS10);			// Wave form generation: WGM13 = 0 & WGM12 = 1, No pre-sacale == CS12 =0, CS11 = 0, CS12 =1
	TCCR1C = 0x00;							// No forced compare
	TCNT1 = 0;								// Initial value for CTC counter


	// Set up Timer/Counter3:CTC mode, output pin does not toggle, no prescaling, 
	// siren update frequency = (16,000,000)/(OCR3A) ~ set to about 1000 cycles/sec

	TCCR3A =(1<<COM3A1)|(1<<COM3A0);		// Compare variable OCR1A turned On, Wave form generation: WGM11 = 0 & WGM12 = 0
	TCCR3B =(1<<WGM32)| (1<<CS30);			// Wave form generation: WGM13 = 0 & WGM12 = 1, No pre-sacale == CS12 =0, CS11 = 0, CS12 =1
	TCCR3C = 0x00;							//no forced compare
	TCNT3 = 0;								// Initial value for CTC counter
	OCR3A =	0x1000;							//pick a speed from 0x1000 to 0xF000
	ETIMSK =(1<<OCIE3A);					//enable timer 3 interrupt on OCIE3A
	
	//TCNT2 setup for providing the volume control
	//fast PWM mode, TOP=0xFF, clear on match, clk/128
	//output is on PORTB bit 7
	TCCR2 =  (1<<WGM21) | (1<<WGM20) | (1<<COM21) | (1<<COM20) | (1<<CS20) | (1<<CS21);
	OCR2  = 0x90;  //set to adjust volume control
}
//******************************************************************************************************
//********************	 END Skeletoncode:	 ***************************************************************
//******************************************************************************************************
