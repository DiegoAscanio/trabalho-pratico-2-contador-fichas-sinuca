/* This is an ESP8266 project designed to count coins in a pool table
 * and send this count to an adafruit io mqtt server everytime a coin
 * is inserted in the pool table.
 *
 * We'll count these coins through a LDR sensor that will be placed
 * right after the lever that releases the balls that only opens when
 * a coin is inserted.
 *
 * This lever is mechanically locked so it won't reach its end position
 * and push the drawer (gaveta) that holds the balls if a coin ain't inserted. 
 * So there's a gap between the lever and the drawer that will be used
 * to place the LDR sensor and a light source and any time that a coin
 * is inserted and the player pulls the lever, the light will be blocked
 * and the LDR sensor will change its resistance to a very high value.
 *
 * We'll place the LDR before the base in an BC548 transistor, so when
 * a coin is inserted almost no current will flow through the base and
 * then the collector and emitter will be open, so ESP8266 GPIO2 pin 
 * configured as INPUT_PULLUP will be HIGH.
 *
 * We'll use this transitions (from LOW to HIGH and from HIGH to LOW
 * if necessary) to make state transitions in a finite state machine
 * that will count the coins and send this count to the adafruit io
 * mqtt server.
 *
 * You should place WiFi and Adafruit IO credentials inside the 
 * secrets.h file.
 *
 * Finite state machine description:
 *
 * There are three states in this FSM:
 *     - Drawer Closed: the drawer is closed and no coin was inserted. While
 *       the LDR sensor identifies that the light is on (the lever is not
 *       pulled) the FSM should stay in this state. If the light goes off
 *       (the lever is pulled) then the FSM should go to the next state,
 *       Drawer Open.
 *     - Drawer Open: the drawer is open and a coin was inserted. While the
 *       LDR sensor identifies that the light is off (the lever is pulled)
 *       the FSM should stay in this state. If the light goes on (the lever
 *       is not pulled) the coin count should be increased, this count should
 *       be reported through MQTT and then the FSM should go back to the
 *       Drawer Closed state.
 *       If the light is off for too long (timeout of 30 minutes is reached)
 *       then the FSM should go to the Open For Too Long state.
 *     - Open For Too Long: the drawer is open for too long so the table is
 *       unavailable for other players to play. So the FSM should stay in this
 *       state until the drawer is closed and report at every 5 minutes that
 *       the pool table is unavailable due to the drawer being open for too long.
 *       When the drawer is closed the FSM should go back to the Drawer Closed
 *       state, increasing the coin count by 1 and reporting this count through
 *       MQTT.
 *
 * You should use non-blocking timers to implement the necessary timeout
 * verification and the 5 minutes report as if you use delay() function
 * you'd block the ESP8266 from doing other tasks like connecting to the
 * WiFi network or sending the coin count to the adafruit io mqtt server.
 *
 * ESP8266 timers libraries are allowed to be used in this project, although
 * there will be a bonus if you implement your code without using these libraries.
 *
 * You also should assume that Inputs, Outputs, Wifi and MQTT connections are working fine.
 *
 * Any questions about the project should be asked through my e-mail address
 * (ascanio@cefetmg.br), at our whatsapp group, in the classroom, at my
 * office hours or at my whatsapp number.
 *
 * PS: students should organize themselves in groups of 1 to 7 students,
 * this project is evaluated in 10 points and can be presented in our
 * labs until August 21st, 2024.
 */

// inlude the necessary libraries
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "secrets.h"
#include <ESP8266WiFi.h>
#include <Ticker.h>

// define the TIMEOUT in which the drawer is open for too long and also
// used to report that the drawer is open for too long every 5 minutes
int TIMEOUT=18;

// define a float variable to store the current time that the drawer is
// open.
float openTime = 0;

// define a new type through enum to represent the states of the FSM
enum State {
    DRAWER_CLOSED,
    DRAWER_OPEN,
    OPEN_FOR_TOO_LONG
};

// create a variable to represent the FSM and its current state
// the initial and also the end state is DRAWER_CLOSED
State state = DRAWER_CLOSED;

// Create a variable to store the coin count
int coinCount = 0;

// Create a variable to store 

/* Create a ticker object to use ESP8266 timers
 * we'll be using it all along the project to create
 * non blocking code to perform states transitions
 */
Ticker ticker;

// Create an ESP8266 WiFiClient object to connect to the WiFi network
WiFiClient client;

// Create an Adafruit IO MQTT client object to connect to the Adafruit IO
// MQTT server
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Create a coinCounterPublisher to report coins count
Adafruit_MQTT_Publish coinCounterPublisher = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/coin-counter");

// Create a reporterPublisher to report that the pool table is open for too long when appropriate
Adafruit_MQTT_Publish reporterPublisher = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/reporter");

void activateTimerIfNotActive(int timeout, void (*callback)()) {
  // attach the callback function to the timer
  // if it's not already attached
  // parameters: timeout: int - the time in milliseconds
  //             callback: void (*)() - the function to be called
  if (!ticker.active()) {
    ticker.attach_ms(timeout, callback);
  }
}

void deactivateTimer() {
  // detach the timer
  ticker.detach();
}

void process_DRAWER_CLOSED_state() {
  openTime = 0;
  // attach this function to the timer if it's not already attached
  // and this function will run every 100ms 
  activateTimerIfNotActive(100, process_DRAWER_CLOSED_state);

  // verify if the LDR sensor is HIGH, so the lever is pulled
  // the drawer is open, a coin is inserted and the light is off
  if (digitalRead(2) == HIGH) {
    Serial.println("Drawer is open");
    // deactivate the timer
    deactivateTimer();
    // change the state to DRAWER_OPEN
    state = DRAWER_OPEN;
    // and process the next state DRAWER_OPEN
    process_DRAWER_OPEN_state();
    return;
  }
}

void process_DRAWER_OPEN_state() {
  // attach this function to the timer if it's not already attached
  // to run every 100ms
  activateTimerIfNotActive(100, process_DRAWER_OPEN_state);
  // each time this functions is called (at each 100ms) we'll
  // increase the openTime variable by 0.1
  openTime += 0.1;
  // if the pool table is open for too long
  // we should change the state to OPEN_FOR_TOO_LONG
  if (openTime >= TIMEOUT) {
    // deactivate the timer
    deactivateTimer();
    // change the state to OPEN_FOR_TOO_LONG
    state = OPEN_FOR_TOO_LONG;
    // set TIMEOUT to 300 seconds
    TIMEOUT = 3;
    // set openTime to 0
    openTime = 0;
    // report that the pool table entered in the OPEN_FOR_TOO_LONG state
    // through MQTT
    reporterPublisher.publish("Pool table is open for too long");
    // and process the next state OPEN_FOR_TOO_LONG
    process_OPEN_FOR_TOO_LONG_state();
    return;
  }
  // verify if the LDR sensor is LOW, so the drawer was closed
  // and then, we can increase the coin count, report this count
  // through MQTT and change the state to DRAWER_CLOSED
  if (digitalRead(2) == LOW) {
    // deactivate the timer
    deactivateTimer();
    coinCount++;
    // report the coin count through MQTT
    coinCounterPublisher.publish(coinCount);
    // report through Serial also
    Serial.println("Coin count: " + String(coinCount));
    // change the state to DRAWER_CLOSED
    state = DRAWER_CLOSED;
    // and process the next state DRAWER_CLOSED
    process_DRAWER_CLOSED_state();
    return;
  }
}

void process_OPEN_FOR_TOO_LONG_state() {
  // attach this function to the timer if it's not already attached
  // to run every 100ms
  activateTimerIfNotActive(100, process_OPEN_FOR_TOO_LONG_state);
  // increase openTime by 0.1
  openTime += 0.1;
  // verify if the LDR sensor is LOW, so the drawer was closed
  // and then, we can change the state to DRAWER_CLOSED
  if (digitalRead(2) == LOW) {
    // deactivate the timer
    deactivateTimer();
    // increase the coin count by 1
    coinCount++;
    // report the coin count through MQTT
    coinCounterPublisher.publish(coinCount);
    // report through Serial also
    Serial.println("Coin count: " + String(coinCount));
    // report that the pool table drawer was finally closed
    reporterPublisher.publish("Pool table drawer was finally closed");
    // Also report through Serial
    Serial.println("Pool table drawer was finally closed");
    // change the state to DRAWER_CLOSED
    state = DRAWER_CLOSED;
    // reset the TIMEOUT to 1800 seconds
    TIMEOUT = 18;
    // set openTime to 0
    openTime = 0;
    // and process the next state DRAWER_CLOSED
    process_DRAWER_CLOSED_state();
    return;
  }
  // now verify if the drawer keeps open for too long
  // and report this through MQTT every 5 minutes
  if (openTime >= TIMEOUT) {
    // report that the pool table is open for too long
    reporterPublisher.publish("Pool table keeps open for too long");
    // report through Serial too
    Serial.println("Pool table keeps open for too long");
    // reset openTime to 0
    openTime = 0;
  }
}

void connectToWifi() {
  // connect to the WiFi network
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void setup() {
  // start the serial communication
  Serial.begin(115200);
  // add a little delay for the serial communication to start
  delay(50);
  // connect to the WiFi network
  connectToWifi();
  // perform the MQTT connection
  MQTT_connect();
  // set the GPIO2 pin as INPUT_PULLUP
  pinMode(2, INPUT_PULLUP);
  // process the first state of the FSM
  process_DRAWER_CLOSED_state();
}

void loop() {
    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    MQTT_connect();
    // ping the server to keep the mqtt connection alive
    if (! mqtt.ping()) {
      mqtt.disconnect();
    }
}
