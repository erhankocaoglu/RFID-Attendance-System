#include <xc.h>

#define _XTAL_FREQ 20000000

#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

#define GREEN_LED PORTAbits.RA0
#define RED_LED   PORTAbits.RA1
#define LCD_RS    PORTCbits.RC0
#define LCD_E     PORTDbits.RD0

void set_leds(unsigned char greenValue, unsigned char redValue)
{
    GREEN_LED = greenValue;
    RED_LED = redValue;
}

void blink_led(unsigned char greenSelected, unsigned char count)
{
    unsigned char i;

    for(i = 0; i < count; i++)
    {
        if(greenSelected)
        {
            GREEN_LED = 1;
        }
        else
        {
            RED_LED = 1;
        }

        __delay_ms(150);

        if(greenSelected)
        {
            GREEN_LED = 0;
        }
        else
        {
            RED_LED = 0;
        }

        __delay_ms(150);
    }
}

void initialize_ports(void)
{
    ADCON1 = 0b00000110;

    TRISA = 0b11111100;    // RA0 and RA1 output
    TRISB = 0b11111111;
    TRISC = 0b11111110;    // RC0 output
    TRISD = 0b00000000;    // LCD output

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
}

void lcd_pulse_enable(void)
{
    LCD_E = 1;
    __delay_us(10);
    LCD_E = 0;
    __delay_us(200);
}

void lcd_send_nibble(unsigned char value)
{
    PORTD = (PORTD & 0x0F) | (value & 0xF0);
    lcd_pulse_enable();
}

void lcd_send(unsigned char value, unsigned char isData)
{
    LCD_RS = isData;

    lcd_send_nibble(value & 0xF0);
    lcd_send_nibble((value << 4) & 0xF0);

    if(isData)
    {
        __delay_us(200);
    }
    else
    {
        __delay_ms(3);
    }
}

void lcd_print(const char *text)
{
    while(*text)
    {
        lcd_send(*text, 1);
        text++;
    }
}

void lcd_clear(void)
{
    lcd_send(0x01, 0);
    __delay_ms(3);
}

void lcd_set_cursor(unsigned char row, unsigned char column)
{
    if(row == 1)
    {
        lcd_send(0x80 + column - 1, 0);
    }
    else
    {
        lcd_send(0xC0 + column - 1, 0);
    }
}

void lcd_show_message(const char *firstLine, const char *secondLine)
{
    lcd_clear();

    lcd_set_cursor(1, 1);
    lcd_print(firstLine);

    lcd_set_cursor(2, 1);
    lcd_print(secondLine);
}

void initialize_lcd(void)
{
    __delay_ms(50);

    LCD_RS = 0;
    LCD_E = 0;

    lcd_send_nibble(0x30);
    __delay_ms(5);

    lcd_send_nibble(0x30);
    __delay_ms(5);

    lcd_send_nibble(0x30);
    __delay_ms(5);

    lcd_send_nibble(0x20);
    __delay_ms(5);

    lcd_send(0x28, 0);
    lcd_send(0x0C, 0);
    lcd_send(0x06, 0);

    lcd_clear();
}

void main(void)
{
    initialize_ports();

    set_leds(0, 0);

    // Debug: LED test
    blink_led(1, 2);
    blink_led(0, 2);

    initialize_lcd();

    // Debug: LCD test message
    lcd_show_message("LCD OK", "VERSION 1");

    while(1)
    {
        blink_led(1, 1);
        __delay_ms(700);

        blink_led(0, 1);
        __delay_ms(700);
    }
}
