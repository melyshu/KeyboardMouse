CC = g++
FLAGS = -Wall -std=c++17

keyboard_mouse: Device.o UIDevice.o main.cpp
	$(CC) $(FLAGS) Device.o UIDevice.o main.cpp -o keyboard_mouse

Device.o: Device.h UIDevice.h Device.cpp
	$(CC) $(FLAGS) -c Device.cpp -o Device.o

UIDevice.o: Device.h UIDevice.h UIDevice.cpp
	$(CC) $(FLAGS) -c UIDevice.cpp -o UIDevice.o

clean:
	rm -f *.o keyboard_mouse
