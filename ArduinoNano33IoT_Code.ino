#include <WiFiNINA.h>
#include <Config.h>
#include <Firebase.h>
#include <Firebase_Arduino_WiFiNINA.h>
#include <Firebase_TCP_Client.h>
#include <WCS.h>
#include <SPI.h>
#include <Servo.h>

#define FIREBASE_HOST "vision-fit-cea9a-default-rtdb.asia-southeast1.firebasedatabase.app"  // Firebase URL
#define FIREBASE_AUTH "AIzaSyBzUu74iLWMt-LmEdPaCB399gbPT_Z2DTc"            // Firebase Database Secret or Web API Key

char ssid[] = "Bsnl2439";   // your network SSID (name)
char pass[] = "7550661313"; // your network password
int status = WL_IDLE_STATUS;
WiFiClient client;

Servo servoX;
Servo servoY;

bool workoutStatus = false;
bool gender = true;  // true = male, false = female
float height = 180;
String userType = "normal"; // Default to "normal"
float hipWidth = 44;
float shoulderWidth = 45;
bool emergencyStatus =false;
FirebaseData fbdo;

bool isRaspberryPiConnected = false;  // Track Raspberry Pi connection status

void setup() {
  Serial.begin(9600);
  servoX.attach(7);
  servoY.attach(10);
  servoX.write(100);
  servoY.write(85);
  
  while(!Serial) {;}

  // Initialize WiFi and Firebase
  setup_wifi();
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH, ssid, pass);  // Defined in your firebase.h
  Serial.println("Connected to Wi-Fi and Firebase.");
  Firebase.reconnectWiFi(true);

  // Connect to Raspberry Pi server
  if (client.connect("192.168.18.143", 8000)) { // Raspberry Pi IP and port
    Serial.println("Connected to Raspberry Pi");
    isRaspberryPiConnected = true;
  } else {
    Serial.println("Connection failed.");
    isRaspberryPiConnected = false;
    sendDisconnectionStatusToFirebase();  // Notify Firebase about disconnection
  }
}

void loop() {
  // Fetch data from Firebase
  fetchFirebaseData();
  
  float HandGap = calculateHandGap(height, gender);
  float HandGap2 = calculateHandGap2(height);  // Personalized HandGap for athlete
  float LegGap = calculateLegGap(height, gender);
  float LegGap2 = calculatePersonalisedLegGap(hipWidth);  // Personalized LegGap for athlete

  Serial.println("Gap: " + String(HandGap)); // For debugging

  if (workoutStatus) {
    if (isRaspberryPiConnected && client.available()) {
      String command = client.readStringUntil('\n');
      command.trim();

      if (command == "StartPushup") {
        Serial.println("Pushup Started");

        // If user is athlete, use personalized HandGap, else use normal HandGap
        if (userType == "athlete") {
          servoX.write(100 + (HandGap2 / 2));
          delay(3000);
          servoX.write(100 - (HandGap2 / 2)); // Adjust based on position for left hand
          delay(3000);
        } else {
          servoX.write(100 + (HandGap / 2));
          delay(3000);
          servoX.write(100 - (HandGap / 2)); // Adjust based on position for left hand
          delay(3000);
        }

        // Reset laser position after pushup guidance
        servoX.write(100);
        servoY.write(85);
      }

      else if (command == "StartSquat") {
        Serial.println("Squat Started");

        // If user is athlete, use personalized LegGap, else use normal LegGap
        if (userType == "athlete") {
          servoY.write(106);
          servoX.write(100 + (LegGap2 / 2));
          delay(3000);
          servoX.write(100 - (LegGap2 / 2)); // Adjust based on position for left hand
          delay(3000);
        } else {
          servoY.write(106);
          servoX.write(100 + (LegGap / 2));
          delay(3000);
          servoX.write(100 - (LegGap / 2)); // Adjust based on position for left hand
          delay(3000);
        }

        // Reset laser position after squat guidance
        servoX.write(100);
        servoY.write(106);
      }
    } 
    
  }
  else if (!isRaspberryPiConnected) {
        sendDisconnectionStatusToFirebase(); 
         if(emergencyStatus == true)
    {    String emergencyExercise = fetchEmergencyExerciseFromFirebase();
    
      performEmergencyExercise(emergencyExercise, HandGap, HandGap2, LegGap, LegGap2);
    }
      }else {
    // Fetch emergency exercise and perform the selected exercise
   
    
    Serial.println("Start Workout for guidance");
    servoX.write(100);
    servoY.write(85);
  }

  delay(5000);  // Fetch data every 5 seconds
}


// Calculate hand gap based on height and gender
float calculateHandGap(float height_cm, bool gender) {
  float shoulderWidth = (gender == true) ? height_cm * 0.28 : height_cm * 0.26;
  return shoulderWidth * 1.1; // Hand gap is 10% wider than shoulder width
}

// Personalized hand gap for athlete based on height
float calculateHandGap2(float height_cm) {
  float handGap = height_cm * 0.3;  // Adjust based on athlete's needs
  return handGap;
}

// Calculate leg gap based on height and gender
float calculateLegGap(float height, bool gender) {
  float legGap = (gender == true) ? height * 0.25 : height * 0.20;
  return legGap;
}

// Personalized leg gap for athlete based on hip width
float calculatePersonalisedLegGap(float hipWidth_cm) {
  float legGap = hipWidth_cm * 0.8;  // 80% of hip width as a baseline for gap
  return legGap;
}

// Connect to Wi-Fi
void setup_wifi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }
}

// Fetch data from Firebase
void fetchFirebaseData() {
  // Fetch workout status
  if (Firebase.getBool(fbdo, "user/workoutStarted")) {
    workoutStatus = fbdo.boolData();
    Serial.print("Workout Status: ");
    Serial.println(workoutStatus);
  } else {
    Serial.println("Failed to get workout status.");
  }

  // Fetch gender
  if (Firebase.getBool(fbdo, "user/gender")) {
    gender = fbdo.boolData();
    Serial.print("Gender: ");
    Serial.println(gender ? "Male" : "Female");
  } else {
    Serial.println("Failed to get gender.");
  }

   if (Firebase.getBool(fbdo, "user/emergencyTraining")) {
    emergencyStatus = fbdo.boolData();
    Serial.print("Emergency Status: ");
    Serial.println(emergencyStatus);
  } else {
    Serial.println("Failed to get status.");
  }

  // Fetch height
  if (Firebase.getFloat(fbdo, "user/height")) {
    height = fbdo.floatData();
    Serial.print("Height: ");
    Serial.println(height);
  } else {
    Serial.println("Failed to get height.");
  }

  if (Firebase.getFloat(fbdo, "user/hipWidth")) {
    hipWidth = fbdo.floatData();
    Serial.print("Hip Width: ");
    Serial.println(hipWidth);
  } else {
    Serial.println("Failed to get hipWidth.");
  }

  if (Firebase.getFloat(fbdo, "user/shoulderWidth")) {
    shoulderWidth = fbdo.floatData();
    Serial.print("shoulder Width: ");
    Serial.println(shoulderWidth);
  } else {
    Serial.println("Failed to get shoulder width.");
  }

  // Fetch user type (normal or athlete)
  if (Firebase.getString(fbdo, "user/userType")) {
    userType = fbdo.stringData();
    Serial.print("User Type: ");
    Serial.println(userType);
  } else {
    Serial.println("Failed to get user type.");
  }

}

String fetchEmergencyExerciseFromFirebase() {
  String exercise = "";
  if (Firebase.getString(fbdo, "user/emergencyExercise")) {
    exercise = fbdo.stringData();
    Serial.print("Emergency Exercise: ");
    Serial.println(exercise);
  } else {
    Serial.println("Failed to get emergency exercise.");
  }
  return exercise;
}

void performEmergencyExercise(String exercise, float HandGap, float HandGap2, float LegGap, float LegGap2) {
  if (exercise == "push-up") {
    Serial.println("Starting Pushup Emergency Training");

    // If user is athlete, use personalized HandGap, else use normal HandGap
    if (userType == "athlete") {
      servoX.write(100 + (HandGap2 / 2));
      delay(3000);
      servoX.write(100 - (HandGap2 / 2)); // Adjust based on position for left hand
      delay(3000);
    } else {
      servoX.write(100 + (HandGap / 2));
      delay(3000);
      servoX.write(100 - (HandGap / 2)); // Adjust based on position for left hand
      delay(3000);
    }

    // Reset laser position after pushup guidance
    servoX.write(100);
    servoY.write(85);
  } 
  else if (exercise == "squat") {
    Serial.println("Starting Squat Emergency Training");

    // If user is athlete, use personalized LegGap, else use normal LegGap
    if (userType == "athlete") {
      servoY.write(106);
      servoX.write(100 + (LegGap2 / 2));
      delay(3000);
      servoX.write(100 - (LegGap2 / 2)); // Adjust based on position for left hand
      delay(3000);
    } else {
      servoY.write(106);
      servoX.write(100 + (LegGap / 2));
      delay(3000);
      servoX.write(100 - (LegGap / 2)); // Adjust based on position for left hand
      delay(3000);
    }

    // Reset laser position after squat guidance
    servoX.write(100);
    servoY.write(106);
  }
}

// Send disconnection status to Firebase
void sendDisconnectionStatusToFirebase() {
  if (Firebase.setString(fbdo, "/user/connectionStatus", "centralDisconnected")) {
    Serial.println("Connection status updated to Firebase.");
  } else {
    Serial.println("Failed to update connection status.");
  }
}
