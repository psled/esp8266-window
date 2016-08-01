/*
 *  This sketch demonstrates how to set up a simple HTTP-like server.
 */

#define SERVO_PIN 2
#define FULLY_OPEN_SENSOR_PIN 3
#define FULLY_CLOSE_SENSOR_PIN 0

#include <ESP8266WiFi.h>
#include <Servo.h>

const char* ssid = "yourNetwork";
const char* password = "secretPassword";

WiFiServer server(1337);  // 80

Servo servo;

enum HttpMethod {
  HM_NONE,
  HM_GET,
  HM_POST
};

enum PositionState {
  PS_DECREASING = 0,
  PS_INCREASING = 1,
  PS_STOPPED = 2
};

const int windowPhysicalLength = 120;
const int windowMargin = 4;
const int windowLength = windowPhysicalLength - (2 * windowMargin);

int currentPosition = -1;
int targetPosition = -1;

int previousPosition = -1;

unsigned long onePercentDuration;
unsigned long previousPositionTime;

/*
 * Setup
 */
void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(FULLY_OPEN_SENSOR_PIN, INPUT);
  pinMode(FULLY_CLOSE_SENSOR_PIN, INPUT);

  Serial.println();
  Serial.println();

  // Calculate window opening / closing duration
  Serial.println("Calculating open / close duration...");
  calculateDuration();
  Serial.println("Calculated");

  Serial.println();

  // Connect to WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());
}

void handleRequest(String request, String& response, String route,
    HttpMethod httpMethod, String (*handler)(String)) {
  if (response.length() > 0) {
    return;
  }

  String reqHttpMethodString = shiftStringUntil(request, " ");
  HttpMethod reqHttpMethod = parseHttpMethodString(reqHttpMethodString);

  String reqRoute = shiftStringUntil(request, " ");

  if (reqHttpMethod != httpMethod) {
    return;
  }

  if (reqRoute != route) {
    return;
  }

  shiftStringUntil(request, "\r\n\r\n");
  String requestBody = request;

  response = handler(requestBody);
}

String createResponse(String responseBody) {
  // Prepare the response
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: application/json\r\n";
  response += "\r\n";
  if (responseBody.length() > 0) {
    response += responseBody;
  }
  return response;
}

String createResponse() {
  String responseBody;
  String response = createResponse(responseBody);
  return response;
}

String handleCurrentPosition(String requestBody) {
  String responseBody = String(currentPosition);
  String response = createResponse(responseBody);
  return response;
}

String handlePositionState(String requestBody) {
  PositionState state = PS_STOPPED;
  if (targetPosition != -1) {
    int diff = targetPosition - currentPosition;
    if (diff > 0) {
      state = PS_INCREASING;
    } else if (diff < 0) {
      state = PS_DECREASING;
    }
  }
  String responseBody = String(state);
  String response = createResponse(responseBody);
  return response;
}

String handleTargetPosition(String requestBody) {
  String response = createResponse();

  // TODO: Pretty bad JSON parsing.
  // We should really use a proper JSON parsing library here.
  const String valueKey = "\"value\"";
  int idx = requestBody.indexOf(valueKey);
  if (idx == -1) {
    return response;
  }
  requestBody.remove(0, idx + valueKey.length());

  idx = requestBody.indexOf(':');
  if (idx == -1) {
    return response;
  }
  requestBody.remove(0, idx + 1);

  idx = requestBody.indexOf('}');
  if (idx == -1) {
    return response;
  }
  String newTargetPositionStr = requestBody.substring(0, idx);
  newTargetPositionStr.trim();

  int newTargetPosition = newTargetPositionStr.toInt();

  // if isNaN(newTargetPosition)
  if (newTargetPosition == 0 && newTargetPositionStr != "0") {
    return response;
  }

  if (newTargetPosition < 0 || newTargetPosition > 100
      || newTargetPosition == targetPosition
      || (targetPosition == -1 && newTargetPosition == currentPosition)) {
    return response;
  }

  targetPosition = newTargetPosition;

  Serial.println("Target position: " + targetPosition);

  previousPosition = currentPosition;
  previousPositionTime = millis();

  int diff = targetPosition - currentPosition;
  if (diff > 0) {
    openWindow();
  } else if (diff < 0) {
    closeWindow();
  }

  return response;
}

/*
 * Loop
 */
void loop() {
  // Handle requested target position
  if (targetPosition != -1) {
    unsigned long time = millis();
    int prevDiff = targetPosition - previousPosition;
    int newCurrentPosition = _min(100, _max(0, (previousPosition
      + (prevDiff > 0 ? 1 : -1)
        * ((time - previousPositionTime) / onePercentDuration))));
    if (newCurrentPosition != currentPosition) {
      currentPosition = newCurrentPosition;
      Serial.println("Current position: " + String(currentPosition));
    }
    bool isFullyOpen = isWindowFullyOpen();
    bool isFullyClose = isWindowFullyClose();
    int diff = targetPosition - currentPosition;
    if ((prevDiff > 0 && isFullyOpen)
        || (prevDiff < 0 && isFullyClose)
        || (prevDiff > 0 && diff <= 0 && targetPosition != 100)
        || (prevDiff < 0 && diff >= 0 && targetPosition != 0)) {
      holdWindow();
      if (isFullyOpen) {
        currentPosition = 100;
      } else if (isFullyClose) {
        currentPosition = 0;
      }
      targetPosition = -1;
      previousPosition = -1;
      previousPositionTime = 0;
    }
    delay(1);
  }

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("New client");
  while(!client.available()){
    if (!client.connected()) {
      Serial.println("Client disonnected");
      client.stop();
      return;
    }
    delay(1);
  }

  // Read the first line of the request
  //String req = client.readStringUntil('\r');
  String req;
  while (client.available()) {
    char c = client.read();
    req += c;
  }
  Serial.println();
  Serial.println(req);
  client.flush();

  // Match the request
  String res;

  handleRequest(req, res, "/window/currentPosition", HM_GET,
    handleCurrentPosition);

  handleRequest(req, res, "/window/positionState", HM_GET,
    handlePositionState);

  handleRequest(req, res, "/window/targetPosition", HM_POST,
    handleTargetPosition);

  if (res.length() == 0) {
    Serial.println("Invalid request");
    client.stop();
    return;
  }

  client.flush();

  // Send the response to the client
  client.print(res);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed
}

void holdWindow() {
  Serial.println("Hold window");
  if (servo.attached()) {
    //servo.write(90);
    servo.write(85);
    delay(1000);
    servo.detach();
    delay(1);
  }
}

void closeWindow() {
  Serial.println("Closing window");
  if (!servo.attached()) {
    servo.attach(SERVO_PIN);
    delay(1);
  }
  //servo.write(0);
  //servo.write(55);
  servo.write(75);
  delay(1);
}

void openWindow() {
  Serial.println("Opening window");
  if (!servo.attached()) {
    servo.attach(SERVO_PIN);
    delay(1);
  }
  //servo.write(180);
  //servo.write(115);
  servo.write(95);
  delay(1);
}

void calculateDuration() {
  if (!isWindowFullyOpen()) {
    openWindow();
  }
  while (!isWindowFullyOpen()) {
    delay(1);
  }
  unsigned long calculateStart = millis();
  closeWindow();
  while (!isWindowFullyClose()) {
    delay(1);
  }
  holdWindow();
  unsigned long calculateEnd = millis();
  onePercentDuration = (calculateEnd - calculateStart) / 100;
  currentPosition = 0;
}

String shiftStringUntil(String& s, String untilString) {
  String result;
  int idx = s.indexOf(untilString);;
  if (idx != -1) {
    result = s.substring(0, idx);
    s.remove(0, idx + untilString.length());
  }
  return result;
}

HttpMethod parseHttpMethodString(String httpMethodStr) {
  HttpMethod result;
  if (httpMethodStr == "GET") {
    result = HM_GET;
  } else if (httpMethodStr == "POST") {
    result = HM_POST;
  } else {
    result = HM_NONE;
  }
  return result;
}

bool isPinHigh(int pin) {
  int val = digitalRead(pin);
  delay(1);
  bool result = val == HIGH ? true : false;
  return result;
}

bool isWindowFullyOpen() {
  bool result = !isPinHigh(FULLY_OPEN_SENSOR_PIN);
  return result;
}

bool isWindowFullyClose() {
  bool result = !isPinHigh(FULLY_CLOSE_SENSOR_PIN);
  return result;
}
