/******************************************************************************
Example for controlling a Smart_Radio using an MQTT switch

Add this to your configuration.yaml in Home assistant:

switch:
  - platform: mqtt
    name: smartradio
    state_topic: "radio/status"
    command_topic: "radio/switch"
    qos: 1
    payload_on: "ON"
    payload_off: "OFF"
    state_on: "ON"
    state_off: "OFF"
    optimistic: true

If you have any problem to persistent/retain status in the topic. Reset it with this command from the broker console:

sudo mosquitto_pub -h 127.0.0.1 -t radio/switch -n -r -d

******************************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>

const char *ssid =  "xxxxxxx";   // name of your WiFi network
const char *password =  "xxxxxxx"; // password of the WiFi network
const byte ampl_power = 26;// relay pin
const byte SWITCH_PIN = 14;// Pin to control the light with
const char *ID = "smartradio";  // Name of our device, must be unique
const char *TOPIC_switch = "radio/switch";  // Topic to subcribe to
const char *TOPIC_status = "radio/status";  // Topic to subcribe to
unsigned long previousMillis = 0;
unsigned long interval = 180000;
bool state=0;
bool count_check_MQTT = 0;
bool power_safe_state=0;
int count=0;
int wifi_down=0;

#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  300      /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

IPAddress broker(192,168,1,150); // IP address of your MQTT broker eg. 192.168.1.150
WiFiClient wclient;
PubSubClient client(wclient); // Setup MQTT client



void setup() {
  Serial.begin(115200); // Start serial communication at 115200 baud
  pinMode(ampl_power, OUTPUT); // Configure ampl_power as an output
  pinMode(SWITCH_PIN, INPUT);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_14,1);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
  print_wakeup_reason();
  setup_wifi(); // Connect to network
  setup_MQTT();
}

//esp32 wakeup from sleep status reason
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by light sleep: %d\n",wakeup_reason); break;
  }
}

//Connect to MQTT broker
void setup_MQTT() {
  client.setServer(broker, 1883);
  client.connect(ID);
  if(client.connect(ID)) {
      client.subscribe(TOPIC_switch);
      Serial.println("connected");
      Serial.print("Subcribed to: ");
      Serial.println(TOPIC_switch);
      Serial.println('\n');
      }else {
           Serial.println(" MQTT broker unavailable");
           Serial.println(" try again in 3 minutes");
    }
  client.setCallback(callback);// Initialize the callback routine
}

//If the MQTT broker is not connected, try to reconnect. Leave the current amplifier status unchanged
void MQTT_reconnect() {
  if (!client.connected()) {
    if (state == 1) {
      if (!client.connected()) {
          Serial.print("ON - Attempting MQTT connection...");
          if(client.connect(ID)) {
            client.subscribe(TOPIC_switch);
            Serial.println("connected");
            Serial.print("Subcribed to: ");
            Serial.println(TOPIC_switch);
            Serial.println('\n');
            client.publish(TOPIC_switch, "ON");
            client.publish(TOPIC_status,"ON");
            } else {
               Serial.println("ON state - MQTT broker unavailable, try again in 3 minutes");
                 }
           }  
     }
      if (state == 0) {
        if (!client.connected()) {
          Serial.print("OFF - Attempting MQTT connection...");
          if(client.connect(ID)) {
            client.subscribe(TOPIC_switch);
            Serial.println("connected");
            Serial.print("Subcribed to: ");
            Serial.println(TOPIC_switch);
            Serial.println('\n');
            client.publish(TOPIC_switch, "OFF");
            client.publish(TOPIC_status,"OFF");
            } else {
               Serial.println("OFF state - MQTT broker unavailable, try again in 3 minutes");
                 }
           }   
      }
    }
  
}

//Check every 3 min. If the MQTT broker is not connected, try to reconnect. Leave the current amplifier status unchanged
void MQTT_reconnect_timer() {
  if (millis() - previousMillis > interval) {
       previousMillis = millis();
       MQTT_reconnect();
 }
}


// Handle incomming messages from the broker
void callback(char* topic, byte* payload, unsigned int length) {
  String response;
  for (int i = 0; i < length; i++) {
    response += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(response);
  if(response == "ON")  // Turn the amplifier on
  {
    digitalWrite(ampl_power, HIGH);
    client.publish(TOPIC_status,"ON");
    state = 1;
    delay(20);
  }
  else if(response == "OFF")  // Turn the amplifier off
  {
    digitalWrite(ampl_power, LOW);
    client.publish(TOPIC_status,"OFF");
    state = 0;
    delay(20);
  }
}

void check_MQTT_broker() {
     if ((!client.connected()) && (count_check_MQTT == 0))  {
        Serial.print("MQTT broker UNAVAILABLE");
        Serial.println('\n');
        count_check_MQTT = 1;
     }
     if ((client.connected())  && (count_check_MQTT == 1)) {
        count_check_MQTT = 0;
     }
}

// Connect to WiFi network
void setup_wifi() {
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Connect to network
  while ((WiFi.status() != WL_CONNECTED) && (count < 30)){
    delay(300);
    Serial.print(".");
    count++;
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

//if wakeup ESP32 manually via switch button, switch ON the amplifier, if not leave amplifier OFF
void power_safe_recovery() {
      if(power_safe_state == 1) {
         print_wakeup_reason();
         delay(250);
         esp_sleep_wakeup_cause_t wakeup_reason;
         wakeup_reason = esp_sleep_get_wakeup_cause();
        if (WiFi.status() != WL_CONNECTED) { // Wait for connection
         setup_wifi();
         }
        if (WiFi.status() == WL_CONNECTED) {
         wifi_down = 0;
        }
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
          digitalWrite(ampl_power, HIGH);
          (state = 1);
          MQTT_reconnect();
          }
        if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
          MQTT_reconnect();
          }       
        power_safe_state=0;
        }
}

//Start power safe mode when the amplifier is off and MQTT broker is unreacheble
void power_safe() {
     if((WiFi.status() != WL_CONNECTED)) {
     wifi_down = 1;
     state = 0;
     digitalWrite(ampl_power, LOW);
     Serial.println("Power safe mode ENABLED because WIFI NETWORK NOT CONNECTED");
     power_safe_state=1;
     delay(1000);
     Serial.flush();
     esp_light_sleep_start();     
     }
     if((state == 0) && (!client.connected()) && (wifi_down == 0)) {
     Serial.println("Power safe mode ENABLED - LIGHT SLEEP");
     power_safe_state=1;
     delay(1000);
     Serial.flush();
     esp_light_sleep_start();     
     }
}

//Physical button switch ON/OFF
void switchON_button() {
  if(digitalRead(SWITCH_PIN) == 1)
  {
    state = !state; //toggle state
    if(state == 1) // ON
     {
      client.publish(TOPIC_switch, "ON");
      client.publish(TOPIC_status,"ON");
      digitalWrite(ampl_power, HIGH); //Turn the amplifier on
      Serial.println((String)TOPIC_switch + " => ON");
      delay(50);
    }
    else // OFF
    {
      client.publish(TOPIC_switch, "OFF");
      client.publish(TOPIC_status, "OFF");
      digitalWrite(ampl_power, LOW);   //Turn the amplifier off
      Serial.println((String)TOPIC_switch + " => OFF");
      delay(50);
    }

    while(digitalRead(SWITCH_PIN) == 1) // Wait for switch to be released
    {
      // Let the ESP handle some behind the scenes stuff if it needs to
      delay(20);
    }
  }
}


void loop() {
      power_safe_recovery();
      switchON_button();
      check_MQTT_broker();
      MQTT_reconnect_timer();
      client.loop();
      power_safe();
}
