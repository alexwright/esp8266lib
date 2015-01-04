::avr-gcc -g -Os -mmcu=atmega328p -c softuart.c -DF_CPU=16000000
avr-gcc -g -Os -mmcu=atmega328p -c esp8266.c -DF_CPU=16000000
avr-gcc -g -Os -mmcu=atmega328p -c main.c -DF_CPU=16000000
::avr-gcc -g -mmcu=atmega328p -o connect.elf main.o softuart.o
avr-gcc -g -mmcu=atmega328p -o connect.elf main.o esp8266.o
avr-objcopy -j .text -j .data -O ihex connect.elf connect.hex