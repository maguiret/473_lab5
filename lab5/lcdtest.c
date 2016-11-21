#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
//#include "hd44780.h"

// RS bit = 1
#define LCD_DATA(d) {		\
	SPDR = 0x01;				\
	while (!(SPSR & 0x80)) {}	\
	SPDR = (d);					\
	while (!(SPSR & 0x80)) {}	\
	PORTF |= 0x08;				\
	PORTF &= ~0x08;				\
	_delay_us(100);				\
}

// RS bit = 0
#define LCD_CMD(c) {			\
	SPDR = 0x00;				\
	while (!(SPSR & 0x80)) {}	\
	SPDR = (c);					\
	while (!(SPSR & 0x80)) {}	\
	PORTF |= 0x08;				\
	PORTF &= ~0x08;				\
	_delay_us(100);				\
}

void LCD_Init(void){
   /*
                Setup External SRAM configuration
                $0460 - $7FFF / $8000 - $FFFF
                Lower page wait state(s): None
                Upper page wait state(s): 2r/w
        */
//Commented out the following three lines to prevent downstream breakage
//R. Traylor 10.24.2016
//      MCUCR=0x80;
//      XMCRA=0x42;
//      XMCRB=0x80;

        /*
                Initialize the LCD screen
        */

        // Set the SPI settings
        LCD_SPIInit();

        DDRF |= 0x08;  // port F bit 3 is the enable strobe for the LCD
        _delay_ms(15);

        // request 8 bit interface mode
        LCD_CMD(0x38);
        _delay_ms(5);

        // display off
        LCD_CMD(0x08);
        _delay_ms(2);

        // choose entry mode so that the cursor is incremented
        LCD_CMD(0x06);

        /*
                We can add up to 8 custom characters to the LCD
                They can be accessed via ASCII characters 0..7
                Each digit on the display uses a 5x8 matrix
                In order to customize the icon, you specify 5 bits
                for each of the 8 rows.
        */
        // Create a custom battery logo in slot 0
        LCD_CMD(0x40); // <-- address of custom slot 0
        LCD_DATA(0x0E); // <-- top 5 pixels of icon
        LCD_DATA(0x1B); // <-- next 5 pixels of icon
        LCD_DATA(0x11);
        LCD_DATA(0x11);
        LCD_DATA(0x11);
        LCD_DATA(0x11);
        LCD_DATA(0x11);
        LCD_DATA(0x1F); // <-- bottom 5 pixels of icon

        /*
                Clear the screen and enable the LCD
        */
        // clear display
        LCD_CMD(0x01);
        _delay_ms(5);

        // display on
        LCD_CMD(0x0C);
}

void LCD_SPIInit(void) {
        DDRF |= 0x08;  //port F bit 3 is enable for LCD
        PORTB |= 0x00; //port B initialization for SPI
        DDRB |= 0x07;  //Turn on SS, MOSI, SCLK
        //Master mode, Clock=clk/2, Cycle half phase, Low polarity, MSB first
        SPCR = 0x50;
        SPSR = 0x01;
}

void LCD_PutStr(char *lcd_str) {
        uint8_t count;
        for (count=0; count<=(strlen(lcd_str)-1); count++){
                LCD_DATA(lcd_str[count]);
        }

}

int main(){
	DDRF |= 0x08;  //port F bit 3 is enable for LCD
	PORTB |= 0x00; //port B initialization for SPI
	DDRB |= 0x07;  //Turn on SS, MOSI, SCLK
	//Master mode, Clock=clk/2, Cycle half phase, Low polarity, MSB first
	SPCR = 0x50;
	SPSR = 0x01;
        uint8_t i = 0;

  while(1){
        _delay_ms(15);
       // request 8 bit interface mode
        SPDR = 0x00;                            
        while (!(SPSR & 0x80)) {}       
        SPDR = 0x38;                                     
        while (!(SPSR & 0x80)) {}       
        PORTF |= 0x08;                          
        PORTF &= ~0x08;                        
        _delay_us(100);                       


//	LCD_CMD(0x38);
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
//	LCD_CMD(0x01);
	_delay_ms(5);
	
	// display on
        SPDR = 0x00;                            
        while (!(SPSR & 0x80)) {}       
        SPDR = 0x0C;                                     
        while (!(SPSR & 0x80)) {}       
        PORTF |= 0x08;                          
        PORTF &= ~0x08;                        
        _delay_us(100);                       
//	LCD_CMD(0x0C);
	

        

    LCD_PutStr("REFLEX TESTER");
  }
}
