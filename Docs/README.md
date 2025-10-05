# Desk Health IoT Assistant 

An IoT project to encourage healthier desk habits — reminding you to drink water, maintain good posture, and monitor air comfort.  

![Prototype Photo](images/prototype.png)  
*Physical prototype with sensors connected to NodeMCU.*

![Dashboard Demo](images/dashboard.gif)  
*Adafruit IO dashboard with live sensor data and mode controls.*

---

## Overview
Following the shift to remote work and study, people often spend hours at their desks without breaks.  
This project uses sensors + MQTT + Adafruit IO to provide real-time feedback on hydration, posture, and air comfort.

- **NodeMCU ESP8266** for WiFi + MQTT communication
- **DHT11 sensor** → Temperature & Humidity  
- **Moisture sensor** → Detects presence of a water glass  
- **Ultrasonic distance sensor** → Monitors screen distance  
- **Adafruit IO** → Dashboard, controls, and alerts  


##  Features
-  Real-time data logging to Adafruit IO  
-  Custom “modes” (Focus, Relax, Sleep) that adjust thresholds  
-  Alerts for hydration, screen distance, and air comfort  
-  Simulated + physical prototypes  

---

##  Hardware Setup
- **NodeMCU ESP8266**  
- **Breadboard**  
- **DHT11 Temp/Humidity Sensor**  
- **Soil Moisture Sensor** (used for water detection)  
- **HC-SR04 Ultrasonic Distance Sensor**  

---

## 🚀 How to Run
1. Clone this repo:
   ```bash
   git clone https://github.com/yourusername/desk-health-assistant.git
   cd desk-health-assistant

    Add your Adafruit IO credentials in secrets.h (Arduino) or export them as env variables for the Python simulator.

    Run the Python simulator:

    cd Simulators/py
    source venv/bin/activate
    python desk_health_sim.py

    Or upload the Arduino code to NodeMCU.