compile:
	arduino-cli compile --fqbn esp8266:esp8266:generic primeiros-passos-LDR

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:generic primeiros-passos-LDR

install: upload

all: compile
