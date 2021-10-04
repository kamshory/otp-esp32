#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>


xTaskHandle Task1;
xTaskHandle Task2;

// LED pins
const int led1 = 2;
const int led2 = 4;



// Replace with your network credentials
char* ssid = "Planetbiru";
char* password = "monyetbuluk";

// Set web webServer port number to 80
WiFiServer webServer(80);

// Variable to store the HTTP request
String header;

PubSubClient mqttClient(espClient);

// Auxiliar variables to store the current output state
String output26State = "off";
String output27State = "off";

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

String mqttTopic = "";
String mqttServer = "192.168.1.15";
String mqttClientId = "1";
String mqttUsername = "user";
String mqttPassword = "pass";

class HTTPHeader
{
private:
  /* data */
  int httpStatus = 200;
  String httpVersion = "1.1";
  String contentType = "text/html";
  int contentLength = 0;
  String arrayStatus[41] = {
    "100 Continue",
    "101 Switching Protocols",
    "200 OK",
    "201 Created",
    "202 Accepted",
    "203 Non-Authoritative Information",
    "204 No Content",
    "205 Reset Content",
    "206 Partial Content",
    "300 Multiple Choices",
    "301 Moved Permanently",
    "302 Found",
    "303 See Other",
    "304 Not Modified",
    "305 Use Proxy",
    "306 (Unused)",
    "307 Temporary Redirect",
    "400 Bad Request",
    "401 Unauthorized",
    "402 Payment Required",
    "403 Forbidden",
    "404 Not Found",
    "405 Method Not Allowed",
    "406 Not Acceptable",
    "407 Proxy Authentication Required",
    "408 Request Timeout",
    "409 Conflict",
    "410 Gone",
    "411 Length Required",
    "412 Precondition Failed",
    "413 Request Entity Too Large",
    "414 Request-URI Too Long",
    "415 Unsupported Media Type",
    "416 Requested Range Not Satisfiable",
    "417 Expectation Failed",
    "500 Internal Server Error",
    "501 Not Implemented",
    "502 Bad Gateway",
    "503 Service Unavailable",
    "504 Gateway Timeout",
    "505 HTTP Version Not Supported"
  };
public:
  HTTPHeader(/* args */);
  ~HTTPHeader();
  String toString()
  {
    String sc = String(this->httpStatus);  
    String headers = "";
    String statusLine = "";
    int i;
    for(i = 0; i < sizeof(this->arrayStatus); i++)
    {
      if(this->arrayStatus[i].indexOf(sc) == 0)
      {
        statusLine = "HTTP/" + this->httpVersion + " " + this->arrayStatus[i];
        break;
      }
    }

    headers += (statusLine + "\r\n");
    headers += "Connection: close\r\n";
    headers += ("Content-type: " + contentType + "\r\n");
    if(contentLength > 0)
    {
      String cl = String(contentLength);
      headers += ("Content-length: " + cl + "\r\n");
    }
    return headers;
  }
};

HTTPHeader::HTTPHeader(int httpStatus, String contentType, int contentLength)
{
  this->httpStatus = httpStatus;
  this->contentType = contentType;
  this->contentLength = contentLength;
}

HTTPHeader::~HTTPHeader()
{
}


void setup() {
  Serial.begin(115200); 
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqttCallback);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web webServer
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  webServer.begin();

  //create a task that will be executed in the WebServer() function, with priority 1 
  xTaskCreate(
              WebServer,       /* Task function. */
              "Task1",         /* name of task. */
              10000,           /* Stack size of task */
              NULL,            /* parameter of the task */
              1,               /* priority of the task */
              &Task1           /* Task handle to keep track of created task */
              );                         
 
  //create a task that will be executed in the SubscribeMQTT() function, with priority 1 
  xTaskCreate(
              SubscribeMQTT,   /* Task function. */
              "Task2",         /* name of task. */
              10000,           /* Stack size of task */
              NULL,            /* parameter of the task */
              1,               /* priority of the task */
              &Task2           /* Task handle to keep track of created task */
              );         
  
}

//WebServer: blinks an LED every 1000 ms
void WebServer( void * pvParameters ){
  
  WiFiClient webClient = webServer.available();   // Listen for incoming clients

  if (webClient) {                             // If a new webClient connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the webClient
    while (webClient.connected() && currentTime - previousTime <= timeoutTime) 
    {  
      // loop while the webClient's connected
      currentTime = millis();
      if (webClient.available()) {             // if there's bytes to read from the webClient,
        char c = webClient.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the webClient HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the webClient knows what's coming, then a blank line:
            
            String responseBody = createResponseBody();
            HTTPHeader httpHeader = HTTPHeader(200, "text/html", responseBody.length());
            String responseHeader = httpHeader->toString();
            webClient.println(responseHeader);
            if(responseBody.length() > 0)
            {
              webClient.println("\r\n");
              webClient.println(responseBody);
            }
            
            
            
            break;
          } else 
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    webClient.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void SubscribeMQTT( void * pvParameters ){
  Serial.print("SubscribeMQTT running");
  for(;;){
    if (!mqttClient.connected()) {
      mqttReconnect();
    }
    mqttClient.loop();
  }
}

void mqttCallback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print("Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == mqttTopic) {
    
  }
}



void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe(mqttTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  
}
