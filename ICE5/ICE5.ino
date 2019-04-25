

#include <ESP8266WiFi.h>    //Requisite Libraries . . .
#include "Wire.h"           //
#include <PubSubClient.h>   //
#include <ArduinoJson.h>    //
#include <Adafruit_Sensor.h>
#include <Adafruit_MPL115A2.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_SSD1306.h>

#include <Ethernet.h>

// initialize mpl115a2 sensor
Adafruit_MPL115A2 mpl115a2;

// oled display
Adafruit_SSD1306 oled = Adafruit_SSD1306(128, 32, &Wire);

// pin connected to DH22 data line
#define DATA_PIN 12

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

#define wifi_ssid "University of Washington"  
#define wifi_password "" 
//
//#define wifi_ssid "NETGEAR38"   
//#define wifi_password "imaginarysky003" 

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

WiFiClient espClient;             //blah blah blah, espClient
PubSubClient mqtt(espClient);     //blah blah blah, tie PubSub (mqtt) client to WiFi client


    
char mac[6]; //A MAC address is a 'truly' unique ID for each device

char message[201]; // 201, as last character in the array is the NULL character, denoting the end of the array

#define BUTTON_PIN 5
bool current = false; // button state
bool last = false; // button state
unsigned long timerOne; // timer

typedef struct {  // used if I were a receiver
  int temp;                   

} ALAA;   

ALAA a; 

//By subscribing to a specific channel or topic, we can listen to those topics we wish to hear.
//We place the callback in a separate tab so we can edit it easier
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; // create the buffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!
//  JsonObject& alaa = jsonBuffer.parseObject(payload); //parse it!  
  
  if (!root.success()) { // well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
  
  root.printTo(Serial); //print out the parsed message
  Serial.println(); //give us some space on the serial monitor read out
}

// set up wifi
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}                                     //5C:CF:7F:F0:B0:C1 for example

// Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("theTopic/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  mpl115a2.begin(); //  starts sensing pressure
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  oled.display(); // make the screen display
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // initialize dht22
  dht.begin();
  setup_wifi()  ;
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function
    // text display tests
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  timerOne = millis();
}


void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'
 
  oled.clearDisplay(); // clear display
  oled.setCursor(0,0); // set cursor

  if (millis() - timerOne > 5000) { // every five sec sends temp and humd data
    sensors_event_t event;
    dht.temperature().getEvent(&event);  
    float temp = event.temperature;     
    dht.humidity().getEvent(&event); // dht reads humidit
    float hum = event.relative_humidity;
    char str_humd[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character
    char str_temp[6];
    dtostrf(hum, 5, 2, str_humd); 
    dtostrf(temp, 5, 2, str_temp);
    Serial.print("Reading from DHT: "); Serial.print(str_humd); Serial.println(" %");
    sprintf(message, "{\"temp\":\"%s\", \"humd\":\"%s\"}", str_temp, str_humd);
    
    mqtt.publish("willa/Temp", message);  
    timerOne = millis();

  }
  
  // grab the current state of the button.
  // we have to flip the logic because we are
  // using a pullup resistor.
  if (digitalRead(BUTTON_PIN) == LOW) {
    current = true;
  } else {
    current = false;
  }
  // return if the value hasn't changed
  if (current == last) {
    return;
  }
  
  Serial.print("button state -> ");
  Serial.println(current);

  sprintf(message, "{\"button\":\"%d\"}", current);
  mqtt.publish("willa/Button", message);  // publish the button state
  
  last = current;
}
