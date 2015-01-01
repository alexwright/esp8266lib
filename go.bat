avr-gcc -g -Os -mmcu=atmega328p -c main.c
avr-gcc -g -Os -mmcu=atmega328p -c main.c
avr-gcc -g -mmcu=atmega328p -o connect.elf main.o
avr-objcopy -j .text -j .data -O ihex connect.elf connect.hex