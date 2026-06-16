#include <xc.h>
#include <string.h>

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
#define ComIEnReg        0x02
#define ComIrqReg        0x04
#define ErrorReg         0x06
#define FIFODataReg      0x09
#define FIFOLevelReg     0x0A
#define ControlReg       0x0C
#define BitFramingReg    0x0D
#define ModeReg          0x11
#define TxControlReg     0x14
#define TxASKReg         0x15
#define TModeReg         0x2A
#define TPrescalerReg    0x2B
#define TReloadRegH      0x2C
#define TReloadRegL      0x2D
#define VersionReg       0x37

#define PCD_IDLE         0x00
#define PCD_TRANSCEIVE   0x0C
#define PCD_RESETPHASE   0x0F

#define PICC_REQIDL      0x26
#define PICC_ANTICOLL    0x93

#define MI_OK            0
#define MI_NOTAGERR      1
#define MI_ERR           2

#define MAX_LEN          16
#define STUDENT_COUNT    2

char studentUidList[STUDENT_COUNT][9] = {
    "88049C57",
    "88042E81"
};

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

    TRISA = 0b11111100;
    TRISB = 0b11111101;
    TRISC = 0b10010000;
    TRISD = 0b00000000;

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

unsigned char rc522_to_card(unsigned char command,
                            unsigned char *sendData,
                            unsigned char sendLength,
                            unsigned char *backData,
                            unsigned int *backLength)
{
    unsigned char status = MI_ERR;
    unsigned char interruptEnable = 0x00;
    unsigned char waitInterrupt = 0x00;
    unsigned char interruptFlags;
    unsigned char lastBits;
    unsigned char fifoLength;
    unsigned char i;
    unsigned int timeout;

    if(command == PCD_TRANSCEIVE)
    {
        interruptEnable = 0x77;
        waitInterrupt = 0x30;
    }

    rc522_write(ComIEnReg, interruptEnable | 0x80);
    rc522_change_bits(ComIrqReg, 0x80, 0);
    rc522_change_bits(FIFOLevelReg, 0x80, 1);
    rc522_write(CommandReg, PCD_IDLE);

    for(i = 0; i < sendLength; i++)
    {
        rc522_write(FIFODataReg, sendData[i]);
    }

    rc522_write(CommandReg, command);

    if(command == PCD_TRANSCEIVE)
    {
        rc522_change_bits(BitFramingReg, 0x80, 1);
    }

    timeout = 2000;

    do
    {
        interruptFlags = rc522_read(ComIrqReg);
        timeout--;
    }
    while((timeout != 0) && !(interruptFlags & 0x01) && !(interruptFlags & waitInterrupt));

    rc522_change_bits(BitFramingReg, 0x80, 0);

    if(timeout == 0)
    {
        return MI_ERR;
    }

    if((rc522_read(ErrorReg) & 0x1B) != 0x00)
    {
        return MI_ERR;
    }

    status = MI_OK;

    if(interruptFlags & 0x01)
    {
        status = MI_NOTAGERR;
    }

    if(command == PCD_TRANSCEIVE)
    {
        fifoLength = rc522_read(FIFOLevelReg);
        lastBits = rc522_read(ControlReg) & 0x07;

        if(lastBits)
        {
            *backLength = ((unsigned int)(fifoLength - 1) * 8) + lastBits;
        }
        else
        {
            *backLength = (unsigned int)fifoLength * 8;
        }

        if(fifoLength == 0)
        {
            fifoLength = 1;
        }

        if(fifoLength > MAX_LEN)
        {
            fifoLength = MAX_LEN;
        }

        for(i = 0; i < fifoLength; i++)
        {
            backData[i] = rc522_read(FIFODataReg);
        }
    }

    return status;
}

unsigned char rc522_request_card(unsigned char *cardBuffer)
{
    unsigned int backBits;
    unsigned char status;

    rc522_write(BitFramingReg, 0x07);
    cardBuffer[0] = PICC_REQIDL;

    status = rc522_to_card(PCD_TRANSCEIVE,
                           cardBuffer,
                           1,
                           cardBuffer,
                           &backBits);

    if((status != MI_OK) || (backBits != 0x10))
    {
        return MI_ERR;
    }

    return status;
}

unsigned char rc522_read_uid(unsigned char *uidBytes)
{
    unsigned char i;
    unsigned char checkByte = 0;
    unsigned int backBits;
    unsigned char status;

    rc522_write(BitFramingReg, 0x00);

    uidBytes[0] = PICC_ANTICOLL;
    uidBytes[1] = 0x20;

    status = rc522_to_card(PCD_TRANSCEIVE,
                           uidBytes,
                           2,
                           uidBytes,
                           &backBits);

    if(status == MI_OK)
    {
        for(i = 0; i < 4; i++)
        {
            checkByte = checkByte ^ uidBytes[i];
        }

        if(checkByte != uidBytes[4])
        {
            status = MI_ERR;
        }
    }

    return status;
}

char nibble_to_hex(unsigned char value)
{
    value = value & 0x0F;

    if(value < 10)
    {
        return '0' + value;
    }

    return 'A' + value - 10;
}

void uid_to_text(unsigned char *uidBytes, char *uidText)
{
    unsigned char i;

    for(i = 0; i < 4; i++)
    {
        uidText[i * 2] = nibble_to_hex(uidBytes[i] >> 4);
        uidText[i * 2 + 1] = nibble_to_hex(uidBytes[i]);
    }

    uidText[8] = '\0';
}

signed char find_student_index(char *uidText)
{
    unsigned char i;

    for(i = 0; i < STUDENT_COUNT; i++)
    {
        if(strcmp(uidText, studentUidList[i]) == 0)
        {
            return i;
        }
    }

    return -1;
}

void process_card(char *uidText)
{
    signed char studentIndex;

    studentIndex = find_student_index(uidText);

    // Debug: print UID
    uart_print("UID:");
    uart_print(uidText);
    uart_print("\r\n");

    if(studentIndex < 0)
    {
        set_leds(0, 1);

        lcd_show_message("INVALID CARD", uidText);

        // Debug: invalid card log
        uart_print("EVENT:INVALID,UID:");
        uart_print(uidText);
        uart_print("\r\n");
    }
    else
    {
        set_leds(1, 0);

        lcd_show_message("VALID CARD", uidText);

        // Debug: valid card log
        uart_print("EVENT:VALID,UID:");
        uart_print(uidText);
        uart_print("\r\n");
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
    unsigned char status;
    unsigned char cardBuffer[MAX_LEN];
    unsigned char uidBytes[5];
    char uidText[9];

    unsigned char cardReadLocked = 0;
    unsigned char noCardCounter = 0;

    initialize_ports();

    set_leds(0, 0);
    blink_led(1, 2);
    blink_led(0, 2);

    initialize_lcd();

    lcd_show_message("LCD OK", "UART STARTING");
    __delay_ms(1000);

    initialize_uart();

    uart_print("SYSTEM STARTED\r\n");

    initialize_spi();
    initialize_rc522();

    if((rc522_read(VersionReg) == 0x00) || (rc522_read(VersionReg) == 0xFF))
    {
        show_reader_error_forever();
    }

    lcd_show_message("SCAN CARD", "WAITING...");
    uart_print("RC522 OK\r\n");

    while(1)
    {
        status = rc522_request_card(cardBuffer);

        if(status == MI_OK)
        {
            noCardCounter = 0;

            status = rc522_read_uid(uidBytes);

            if(status == MI_OK)
            {
                uid_to_text(uidBytes, uidText);

                if(cardReadLocked == 0)
                {
                    process_card(uidText);

                    cardReadLocked = 1;
                    __delay_ms(1500);
                }
            }
            else
            {
                set_leds(0, 1);

                lcd_show_message("UID READ ERROR", "");

                // Debug: UID read error log
                uart_print("EVENT:READ_ERROR\r\n");

                __delay_ms(500);
            }
        }
        else
        {
            set_leds(0, 0);

            noCardCounter++;

            if(noCardCounter >= 10)
            {
                cardReadLocked = 0;
                noCardCounter = 0;

                lcd_show_message("SCAN CARD", "WAITING...");
            }
        }

        __delay_ms(100);
    }
}
