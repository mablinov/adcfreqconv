#define F_CPU 10000000UL
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#include <stdlib.h>
#include <stdio.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

/* Begin uart code */
static void setup_uart();

static char usart_rx_complete();
static char usart_tx_complete();
static char usart_dr_empty();
static void atoh(uint8_t arg, char* str);
static void send_string(const char* str);
static void send_stringn(const char* str, int n);
/* End uart code */

/* Begin 7-segment display code */
static void setup_seg_display();

static void set_seg_on(uint8_t idx);
static void set_seg_off(uint8_t idx);
static void set_seg(uint8_t val, uint8_t idx);
static void set_seg_mask(uint8_t mask);

static void set_dig_on(uint8_t idx);
static void set_dig_off(uint8_t idx);
static void set_dig(uint8_t val, uint8_t idx);
static void set_dig_mask(uint8_t mask);

static void enable_select_digit(uint8_t digit);
static void display_digit(uint8_t digit);
/* End 7-segment display code */

/* Begin TC1 code */
static void setup_tc1();
/* End TC1 code */

/* Begin TC2 code */
static void setup_tc2();
/* End TC2 code */

#define TIMEPOINTS_MAX 32

static volatile uint8_t display[4] = {8, 8, 8, 8};
static volatile uint8_t dpoint = 0xff;

static volatile uint32_t edges;
static volatile uint8_t of_cnt;

static volatile uint8_t edgecount_done;

ISR(TIMER1_CAPT_vect)
{
    ++edges;
}

ISR(TIMER1_OVF_vect)
{
    ++of_cnt;

    if(of_cnt == TIMEPOINTS_MAX) {
        edgecount_done = 1;
        TIMSK1 &= ~_BV(ICIE1);
        TIMSK1 &= ~_BV(TOIE1);
        TCCR1B &= ~_BV(CS10);
    }

}

ISR(TIMER2_OVF_vect)
{
    static uint8_t current_digit = 0;

    enable_select_digit(current_digit);
    display_digit(display[3-current_digit]);
    
    if(current_digit == dpoint)
        set_seg_on(7);
    else
        set_seg_off(7);
    
    if(current_digit == 3)
        current_digit = 0;
    else
        ++current_digit;
}

static uint8_t count_digits(uint32_t num) {
    uint8_t digits = 0;

    while(num) {
        ++digits;
        num /= 10;
    }
    
    return digits;
}

#define T_DELTA ((65535.0f / F_CPU) * TIMEPOINTS_MAX)

static uint32_t calculate_freq() {
    float freq = edges / T_DELTA;
    return (uint32_t)freq;
}

static void init() {
    setup_uart();
    send_string("begin\n\r");

    setup_seg_display();

    setup_tc1();
    
    setup_tc2();

    DDRB &= ~_BV(DDB0);

}

static void set_display(uint32_t freq) {
    /* set the decimal point position */
    switch(count_digits(freq)) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            dpoint = 3;
            break;
        case 5:
            dpoint = 2;
            break;
        case 6:
            dpoint = 1;
            break;
        case 7:
            dpoint = 0;
            break;
        default:
            dpoint = 0xff;
            break;
    }
    
    uint8_t digits[10] = {0};
    uint32_t num = freq;
    uint8_t count = 0;
    /* get the four most significant numbers (if possible) */
    while(num && count < 10) {
        digits[count] = num % 10;
        num /= 10;
        ++count;
    }
    
    uint8_t start_frame = 3;

    if(count_digits(freq) > 4)
        start_frame = count_digits(freq) - 1;
    
    /* copy the digits over to the display */
    uint8_t i;
    for(i = 0; i < 4; ++i) {
        display[i] = digits[start_frame-i];
    }
}
    
int main() {
    init();
	sei();

    display_digit(8);
    enable_select_digit(0);

    while(1) {
        if(edgecount_done) {
            uint32_t freq = calculate_freq();

            set_display(freq);
            
            // Reset the counters
            edgecount_done = 0;
            edges = 0;
            of_cnt = 0;

            // Re-enable interrupts            
            TIMSK1 |= _BV(ICIE1);
            TIMSK1 |= _BV(TOIE1);
            TCCR1B |= _BV(CS10);
        }
    }
}

static void setup_tc2() {
    // Set prescaler to 256
//    TCCR2B |= _BV(CS22);
    TCCR2B |= _BV(CS21);
    TCCR2B |= _BV(CS20);

    // Enable overflow interrupt
    TIMSK2 |= _BV(TOIE2);
}


static void setup_tc1() {
    // Configure ICP pin as input
//    DDRB &= ~_BV(DDB0);

    // Enable noise cancelling
    TCCR1B |= _BV(ICNC1);
    // Select rising edge trigger
    TCCR1B |= _BV(ICES1);
    // Select direct clocking
    TCCR1B |= _BV(CS10);

    // enable input capture interrupt
    TIMSK1 |= _BV(ICIE1);
    // enable overflow interrupt
    TIMSK1 |= _BV(TOIE1);
}

static void setup_uart() {
	// Enable tx/rx
	UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
	// Enable 8 bit char size
	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);

	// Set baud rate 9600
	UBRR0H = (unsigned char)(BAUD_PRESCALE >> 8);
	UBRR0L = (unsigned char)BAUD_PRESCALE;

    // Setup interrupts
//	UCSR0B |= _BV(TXCIE0);
//	UCSR0B |= _BV(RXCIE0);
}

static char usart_rx_complete() {
    return UCSR0A & _BV(RXC0);
}

static char usart_tx_complete() {
    return UCSR0A & _BV(TXC0);
}

static char usart_dr_empty() {
    return UCSR0A & _BV(UDRE0);
}

static void atoh(uint8_t arg, char* str) {
    const char lut[] = "0123456789abcdef";
    str[0] = lut[ 0x0F & (arg >> 4) ];
    str[1] = lut[ 0x0F & (arg) ];
}

static void send_string(const char* str) {
    uint8_t i;
    for(i = 0; str[i]; ++i) {
        while(!usart_dr_empty())
            ;
        UDR0 = str[i];
    }
}
static void send_stringn(const char* str, int n) {
    uint8_t i;
    for(i = 0; i < n; ++i) {
        while(!usart_dr_empty())
            ;
        UDR0 = str[i];
    }
}

static void setup_seg_display() {
    DDRB |= _BV(DDB5);
    DDRB |= _BV(DDB4);
    DDRB |= _BV(DDB3);
    DDRB |= _BV(DDB2);
    DDRB |= _BV(DDB1);

    DDRD |= _BV(DDD2);

    DDRD |= _BV(DDD6);
    DDRD |= _BV(DDD7);


    DDRD |= _BV(DDD5);
    DDRD |= _BV(DDD4);
    DDRC |= _BV(DDC5);
    DDRC |= _BV(DDC4);
}

// turn segments ON
static void set_seg_on(uint8_t idx) {
    switch(idx) {
        case 0:
            PORTB |= _BV(PORTB5);
            break;
        case 1:
            PORTB |= _BV(PORTB4);
            break;
        case 2:
            PORTB |= _BV(PORTB3);
            break;
        case 3:
            PORTB |= _BV(PORTB2);
            break;
        case 4:
            PORTB |= _BV(PORTB1);
            break;
        case 5:
            PORTD |= _BV(PORTD2);
            break;
        case 6:
            PORTD |= _BV(PORTD7);
            break;
        case 7:
            PORTD |= _BV(PORTD6);
            break;
        default:
            break;
    }
}

// turn segments OFF
static void set_seg_off(uint8_t idx) {
    switch(idx) {
        case 0:
            PORTB &= ~_BV(PORTB5);
            break;
        case 1:
            PORTB &= ~_BV(PORTB4);
            break;
        case 2:
            PORTB &= ~_BV(PORTB3);
            break;
        case 3:
            PORTB &= ~_BV(PORTB2);
            break;
        case 4:
            PORTB &= ~_BV(PORTB1);
            break;
        case 5:
            PORTD &= ~_BV(PORTD2);
            break;
        case 6:
            PORTD &= ~_BV(PORTD7);
            break;
        case 7:
            PORTD &= ~_BV(PORTD6);
            break;
        default:
            break;
    }
}

static void set_seg(uint8_t val, uint8_t idx) {
    if(val)
        set_seg_on(idx);
    else
        set_seg_off(idx);
}

static void set_seg_mask(uint8_t mask) {
    uint8_t i;
    for(i = 0; i < 8; ++i) {
        set_seg((1 << i) & mask, 7-i);
    }
}

static void display_digit(uint8_t digit) {
    switch(digit) {
        case 0:
            set_seg_mask(0xFC);
            break;
        case 1:
            set_seg_mask(0x60);
            break;
        case 2:
            set_seg_mask(0xDA);
            break;
        case 3:
            set_seg_mask(0xF2);
            break;
        case 4:
            set_seg_mask(0x66);
            break;
        case 5:
            set_seg_mask(0xB6);
            break;
        case 6:
            set_seg_mask(0xBE);
            break;
        case 7:
            set_seg_mask(0xE0);
            break;
        case 8:
            set_seg_mask(0xFE);
            break;
        case 9:
            set_seg_mask(0xF6);
            break;
        default:
            break;
    }
}

// Digits enable/disable
// digits enable
static void set_dig_on(uint8_t idx) {
    switch(idx) {
        case 0:
            PORTD &= ~_BV(PORTD5);
            break;
        case 1:
            PORTD &= ~_BV(PORTD4);
            break;
        case 2:
            PORTC &= ~_BV(PORTC5);
            break;
        case 3:
            PORTC &= ~_BV(PORTC4);
            break;
        default:
            break;
    }
}

static void set_dig_off(uint8_t idx) {
    switch(idx) {
        case 0:
            PORTD |= _BV(PORTD5);
            break;
        case 1:
            PORTD |= _BV(PORTD4);
            break;
        case 2:
            PORTC |= _BV(PORTC5);
            break;
        case 3:
            PORTC |= _BV(PORTC4);
            break;
        default:
            break;
    }
}

static void set_dig(uint8_t val, uint8_t idx) {
    if(val)
        set_dig_on(idx);
    else
        set_dig_off(idx);
}

static void set_dig_mask(uint8_t mask) {
    uint8_t i;
    for(i = 0; i < 4; ++i) {
        set_dig((1 << i) & mask, i);
    }
}

static void enable_select_digit(uint8_t digit) {
    switch(digit) {
        case 3:
            set_dig_mask(0x8);
            break;
        case 2:
            set_dig_mask(0x4);
            break;
        case 1:
            set_dig_mask(0x2);
            break;
        case 0:
            set_dig_mask(0x1);
            break;
        default:
            break;
    }
}

