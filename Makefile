
AVRDUDE_FLAGS = flash:w:main.hex lfuse:w:0xe2:m

CFLAGS=-Wall

adcfreqsampler.hex: adcfreqsampler.elf
	avr-objcopy -j .text -j .data -O ihex adcfreqsampler.elf adcfreqsampler.hex
adcfreqsampler.elf: adcfreqsampler.c
	avr-gcc $(CFLAGS) -O2 -mmcu=atmega328p -o adcfreqsampler.elf adcfreqsampler.c

flash-adcfreqsampler: adcfreqsampler.hex
	sudo avrdude -b 115200 -P /dev/ttyACM0 -c arduino -p m328p -U flash:w:adcfreqsampler.hex

