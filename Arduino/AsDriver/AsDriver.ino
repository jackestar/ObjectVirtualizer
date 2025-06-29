#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Stepper.h>

// Pins
#define LT1 35 // Distance Sensor

// Signals form controller
#define Q1 18 // Restart
#define Q2 19 // Table Rotation
#define Q3 21 // Elevator Up

// Signals to controller
#define I6 2 // EndLoop
#define I7 4 // Stop

// Elev
#define M1_A 13
#define M1_B 12
#define M1_C 14
#define M1_D 27

// Tab
#define M2_A 26
#define M2_B 25
#define M2_C 33
#define M2_D 32

// --- WiFi & Server Configuration ---
const char *ssid = "UserName";
const char *password = "wifiPassword";
const char *serverIp = "192.168.1.100";
const uint16_t serverPort = 5000;

// --- Stepper Motor Constants ---
const uint16_t stepElvRev = 592;  //
const uint16_t stepTabRev = 1800; // Total steps for a full 360-degree rotation

// --- Global State Variables (volatile because they are shared between cores) ---
volatile uint8_t speedElv = 15;
volatile uint8_t speedTab = 10;
volatile uint16_t readThreshold = 3700;
volatile uint16_t stepPerRead = 10;
volatile uint16_t zStepIncrement = 700;
volatile uint16_t logoSampling = 500;

// Calibration variables
volatile int calib_analog_near = 400;
volatile int calib_dist_near = 10;
volatile int calib_analog_far = 3800;
volatile int calib_dist_far = 150;

// Live Status Variables
volatile bool Q1_status = false;
volatile bool Q2_status = false;
volatile bool Q3_status = false;
volatile int32_t Z = 0; // Current Z position in steps

Stepper Elev(stepElvRev, M1_A, M1_C, M1_B, M1_D);
Stepper Tabl(stepTabRev, M2_A, M2_B, M2_C, M2_D);

TaskHandle_t CommTask;

bool waiting = false;

volatile float distance_mm = 0;
volatile float angle_rad = 0;

// --- Communication Task (runs on Core 0) ---
void commLoop(void *parameter)
{
  String serverUrl_status = "http://" + String(serverIp) + ":" + String(serverPort) + "/api/status";
  String serverUrl_esp_status = "http://" + String(serverIp) + ":" + String(serverPort) + "/api/esp_status";

  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;

      // --- 1. Fetch config from server ---
      http.begin(serverUrl_status);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK)
      {
        String payload = http.getString();
        JSONVar config_data = JSON.parse(payload);

        if (JSON.typeof(config_data) != "undefined")
        {
          JSONVar config = config_data["config"];
          // Update local volatile variables
          speedElv = (int)config["speedElv"];
          speedTab = (int)config["speedTab"];
          readThreshold = (int)config["readThreshold"];
          stepPerRead = (int)config["stepPerRead"];
          zStepIncrement = (int)config["zStepIncrement"];
          logoSampling = (int)config["logoSampling"];
          calib_analog_near = (int)config["calib_analog_near"];
          calib_dist_near = (int)config["calib_dist_near"];
          calib_analog_far = (int)config["calib_analog_far"];
          calib_dist_far = (int)config["calib_dist_far"];
        }
      }
      else
      {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();

      // --- 2. Post ESP32 status back to server ---
      JSONVar esp_status_payload;
      esp_status_payload["q1"] = digitalRead(Q1);
      esp_status_payload["q2"] = digitalRead(Q2);
      esp_status_payload["q3"] = digitalRead(Q3);
      esp_status_payload["z_pos"] = Z;
      String json_payload = JSON.stringify(esp_status_payload);

      http.begin(serverUrl_esp_status);
      http.addHeader("Content-Type", "application/json");
      http.POST(json_payload); // We don't need to check the response for this
      http.end();

      // 3 Points

      if (waiting)
      {
        String url = "http://" + String(serverIp) + ":" + String(serverPort) + "/api/data";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        JSONVar dataPoint;
        // distance_mm, angle_rad, Z);
        dataPoint["r"] = distance_mm;
        dataPoint["theta"] = angle_rad;
        dataPoint["z"] = Z;

        String jsonPayload = JSON.stringify(dataPoint);
        int httpCode = http.POST(jsonPayload);
        if (httpCode != HTTP_CODE_OK)
        {
          Serial.printf("[DATA] POST failed, error code: %d\n", httpCode);
        }
        http.end();
        waiting = false;
      }
    }

    // delay(500); // Sync with server
  }
}

// --- Main Setup (runs once on Core 1) ---
void setup()
{
  // Serial
  Serial.begin(115200);

  pinMode(Q1, INPUT_PULLDOWN);
  pinMode(Q2, INPUT_PULLDOWN);
  pinMode(Q3, INPUT_PULLDOWN);

  pinMode(I6, OUTPUT);
  pinMode(I7, OUTPUT);

  pinMode(LT1, ANALOG);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Create communication task on Core 0
  xTaskCreatePinnedToCore(
      commLoop,   /* Task function */
      "CommTask", /* name of task */
      10000,      /* Stack size of task */
      NULL,       /* parameter of the task */
      1,          /* priority of the task */
      &CommTask,  /* Task handle to keep track of created task */
      0);         /* pin task to core 0 */

  delay(2500); // Give comms task time to fetch initial config

  Elev.setSpeed(speedElv);
  Tabl.setSpeed(speedTab);

  Serial.println("Setup complete. Awaiting commands.");
}

// Custom map function for floating point numbers
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// void postDataPoint(float r, float theta, int z_pos) {
//     if (WiFi.status() == WL_CONNECTED) {
//         HTTPClient http;
//         String url = "http://" + String(serverIp) + ":" + String(serverPort) + "/api/data";
//         http.begin(url);
//         http.addHeader("Content-Type", "application/json");

//         JSONVar dataPoint;
//         dataPoint["r"] = r;
//         dataPoint["theta"] = theta;
//         dataPoint["z"] = z_pos;

//         String jsonPayload = JSON.stringify(dataPoint);
//         int httpCode = http.POST(jsonPayload);
//         if (httpCode != HTTP_CODE_OK) {
//              Serial.printf("[DATA] POST failed, error code: %d\n", httpCode);
//         }
//         http.end();
//     }
// }

void loop()
{
  // Update motor speeds if they have changed
  Elev.setSpeed(speedElv);
  Tabl.setSpeed(speedTab);

  // Read input signals
  Q1_status = digitalRead(Q1);
  if (!Q2_status)
    Q2_status = digitalRead(Q2); // Latch Q2 until scan is done
  if (!Q3_status)
    Q3_status = digitalRead(Q3); // Latch Q3 until elevate is done

  if (Q1_status)
  {
    Serial.println("Init (Q1) triggered. Homing Z.");
    // This could be a more complex homing routine with a limit switch
    Z = 0;
    while (digitalRead(Q1))
    {
      Elev.step(48);
    }
  }

  if (Q2_status)
  {
    Serial.println("Rotation Scan (Q2) triggered.");
    bool object_detected = false;

    for (uint16_t current_step = 0; current_step <= stepTabRev; current_step += stepPerRead)
    {
      const uint16_t raw_read = 4096 - analogRead(LT1);
      Serial.printf("Detection at step %d. Angle: %.2f rad, Raw: %d, Dist: %.2f mm\n", current_step, angle_rad, raw_read, distance_mm);

      if (raw_read < readThreshold)
      {
        object_detected = true;
        // Convert raw reading to distance in mm using calibration values
        distance_mm = mapfloat(raw_read, calib_analog_near, calib_analog_far, calib_dist_near, calib_dist_far);
        // Convert current step to angle in radians
        angle_rad = mapfloat(current_step, 0, stepTabRev, 0, 2.0 * PI);

        // Post the data point to the server (blocks the rotation)
        // postDataPoint(distance_mm, angle_rad, Z);
        waiting = true;
      }

      Tabl.step(stepPerRead);
    }

    // Signal completion to external controller
    if (object_detected)
    {
      digitalWrite(I6, HIGH); // EndLoop
      delay(logoSampling);
      digitalWrite(I6, LOW);
    }
    else
    {
      digitalWrite(I7, HIGH); // Stop/Empty
      delay(logoSampling);
      digitalWrite(I7, LOW);
    }

    Q2_status = false; // Reset status, ready for next trigger
    Serial.println("Rotation Scan finished.");
  }

  if (Q3_status)
  {
    Serial.printf("Elevating (Q3) by %d steps.\n", (int)zStepIncrement);
    Elev.step(-zStepIncrement); // Negative for up
    Z += zStepIncrement / 10;  // Update Z position
    Q3_status = false;          // Reset stawtus
    Serial.printf("New Z position: %d\n", (int)Z);
  }
}