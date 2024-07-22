compile:
	arduino-cli compile --fqbn esp8266:esp8266:generic .

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:generic .

install: upload

all: compile
