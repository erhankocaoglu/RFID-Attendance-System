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
#define RC522_CS  PORTCbits.RC2
#define RC522_RST PORTBbits.RB1

#define CommandReg       0x01
#define ModeReg          0x11
#define TxControlReg     0x14
#define TxASKReg         0x15
#define TModeReg         0x2A
#define TPrescalerReg    0x2B
#define TReloadRegH      0x2C
#define TReloadRegL      0x2D
#define VersionReg       0x37

#define PCD_RESETPHASE   0x0F

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

    TRISA = 0b11111100;    // RA0, RA1 output
    TRISB = 0b11111101;    // RB1 output
    TRISC = 0b10010000;    // RC7 RX input, RC4 MISO input
    TRISD = 0b00000000;    // LCD output

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
}

void initialize_uart(void)
{
    SPBRG = 129;
    TXSTA = 0b00100100;
    RCSTA = 0b10010000;
}

void uart_print(const char *text)
{
    while(*text)
    {
        while(!TXSTAbits.TRMT);
        TXREG = *text;
        text++;
    }
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

unsigned char spi_transfer(unsigned char data)
{
    SSPBUF = data;

    while(!SSPSTATbits.BF);

    return SSPBUF;
}

void initialize_spi(void)
{
    RC522_CS = 1;

    SSPSTAT = 0b01000000;
    SSPCON = 0b00100010;
}

void rc522_write(unsigned char address, unsigned char value)
{
    RC522_CS = 0;

    spi_transfer((address << 1) & 0x7E);
    spi_transfer(value);

    RC522_CS = 1;
}

unsigned char rc522_read(unsigned char address)
{
    unsigned char value;

    RC522_CS = 0;

    spi_transfer(((address << 1) & 0x7E) | 0x80);
    value = spi_transfer(0x00);

    RC522_CS = 1;

    return value;
}

void rc522_change_bits(unsigned char address, unsigned char mask, unsigned char setBits)
{
    unsigned char value;

    value = rc522_read(address);

    if(setBits)
    {
        value = value | mask;
    }
    else
    {
        value = value & ((unsigned char)(~mask));
    }

    rc522_write(address, value);
}

void initialize_rc522(void)
{
    unsigned char txControlValue;

    RC522_RST = 0;
    __delay_ms(50);

    RC522_RST = 1;
    __delay_ms(50);

    rc522_write(CommandReg, PCD_RESETPHASE);
    __delay_ms(50);

    rc522_write(TModeReg, 0x8D);
    rc522_write(TPrescalerReg, 0x3E);
    rc522_write(TReloadRegL, 30);
    rc522_write(TReloadRegH, 0);

    rc522_write(TxASKReg, 0x40);
    rc522_write(ModeReg, 0x3D);

    txControlValue = rc522_read(TxControlReg);

    if((txControlValue & 0x03) != 0x03)
    {
        rc522_change_bits(TxControlReg, 0x03, 1);
    }
}

void show_reader_error_forever(void)
{
    lcd_show_message("RC522 ERROR", "NO SPI");
    uart_print("RC522 ERROR\r\n");

    while(1)
    {
        blink_led(0, 1);
    }
}

void main(void)
{
    unsigned char versionValue;

    initialize_ports();

    set_leds(0, 0);
    blink_led(1, 2);
    blink_led(0, 2);

    initialize_lcd();

    lcd_show_message("LCD OK", "UART STARTING");
    __delay_ms(1000);

    initialize_uart();

    // Debug: UART test message
    uart_print("SYSTEM STARTED\r\n");

    initialize_spi();
    initialize_rc522();

    versionValue = rc522_read(VersionReg);

    if((versionValue == 0x00) || (versionValue == 0xFF))
    {
        show_reader_error_forever();
    }

    // Debug: RC522 connection test
    lcd_show_message("RC522 OK", "VERSION 2");
    uart_print("RC522 OK\r\n");

    while(1)
    {
        blink_led(1, 1);
        __delay_ms(1000);
    }
}
