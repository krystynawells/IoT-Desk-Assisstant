import os, time, random
import paho.mqtt.client as mqtt

# ==============================
#   Adafruit IO Credentials
# ==============================
# Pulled from environment variables for security.
# (Set these in your terminal before running the sim).
AIO_USER = os.getenv("AIO_USERNAME", "YOUR_AIO_USER")
AIO_KEY  = os.getenv("AIO_KEY", "YOUR_AIO_KEY")

HOST = "io.adafruit.com"
PORT = 1883
# Delay between sending data points to Adafruit IO.
# Defaults to 15s, but can be overridden with SIM_SLEEP.
SLEEP = int(os.getenv("SIM_SLEEP", "15"))

# ==============================
#   Feed Definitions
# ==============================
# Each "feed" is basically a channel in Adafruit IO
# where we either publish data or listen for control messages.
FEEDS = {
    "temp":   f"{AIO_USER}/feeds/temperature",
    "humid":  f"{AIO_USER}/feeds/humidity",
    "moist":  f"{AIO_USER}/feeds/moisture",
    "dist":   f"{AIO_USER}/feeds/distance",
    "alerts": f"{AIO_USER}/feeds/alerts",
    "mode":   f"{AIO_USER}/feeds/mode",      # dashboard dropdown (focus / relax / sleep)
}

# ==============================
#   Default Thresholds
# ==============================
# These will get overridden when a "mode" is sent in.
CLOSE_DISTANCE_CM = 20        # too close to the screen
HYDRATE_INTERVAL_S = 30 * 60  # 30 minutes
notify_enabled = True         # toggle for alerts
last_drink = time.time()      # track last "hydration event"

# ==============================
#   Mode Handling
# ==============================
def apply_mode(mode: str):
    """
    Change thresholds based on dashboard mode.
    focus -> strict rules
    relax -> moderate
    sleep -> very lenient
    """
    global CLOSE_DISTANCE_CM, HYDRATE_INTERVAL_S
    m = (mode or "").strip().lower()
    if m == "focus":
        CLOSE_DISTANCE_CM = 25
        HYDRATE_INTERVAL_S = 20 * 60
    elif m == "relax":
        CLOSE_DISTANCE_CM = 30
        HYDRATE_INTERVAL_S = 45 * 60
    elif m == "sleep":
        CLOSE_DISTANCE_CM = 15
        HYDRATE_INTERVAL_S = 120 * 60
    else:
        print(f"[mode] unknown '{mode}' (use focus/relax/sleep)")
        return
    print(f"[mode] {m} -> close<{CLOSE_DISTANCE_CM}cm hydrate={HYDRATE_INTERVAL_S//60}min")

# ==============================
#   MQTT Event Handlers
# ==============================
def on_connect(client, userdata, flags, rc):
    """When connected, subscribe to mode changes."""
    print("MQTT connected" if rc == 0 else f"MQTT failed rc={rc}")
    client.subscribe(FEEDS["mode"])

def on_message(client, userdata, msg):
    """Process incoming MQTT messages."""
    topic = msg.topic
    payload = msg.payload.decode(errors="ignore").strip()
    if topic == FEEDS["mode"]:
        apply_mode(payload)

# ==============================
#   Alert Helper
# ==============================
def publish_alert(client, text):
    """Send alerts to dashboard + also print locally."""
    if notify_enabled:
        client.publish(FEEDS["alerts"], text, qos=1)
    print("ALERT:", text)

# ==============================
#   Main Loop
# ==============================
def main():
    global last_drink

    # MQTT setup
    client = mqtt.Client()
    client.username_pw_set(AIO_USER, AIO_KEY)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(HOST, PORT, 60)
    client.loop_start()

    try:
        while True:
            # --- Simulated sensor data ---
            temp = random.uniform(18.0, 30.0)     # °C
            humid = random.randint(30, 70)        # %
            moisture = random.randint(380, 420)   # arbitrary "glass sensor"
            distance = random.randint(10, 50)     # cm

            # Publish telemetry to Adafruit IO
            client.publish(FEEDS["temp"],  f"{temp:.2f}")
            client.publish(FEEDS["humid"], str(humid))
            client.publish(FEEDS["moist"], str(moisture))
            client.publish(FEEDS["dist"],  str(distance))

            # Debug printout so we see live sim state
            print(f"T={temp:.1f}C H={humid}% M={moisture} D={distance}cm "
                  f"[close<{CLOSE_DISTANCE_CM}cm hydrate={HYDRATE_INTERVAL_S//60}min notify={notify_enabled}]")

            # --- Rule logic ---
            now = time.time()

            # "Hydration event" if glass touched
            if moisture > 400:
                last_drink = now

            # Hydrate reminder if too long since last drink
            if now - last_drink > HYDRATE_INTERVAL_S:
                publish_alert(client, "Reminder: Take a sip of water!")
                last_drink = now

            # Too close to screen
            if 0 < distance < CLOSE_DISTANCE_CM:
                publish_alert(client, "You're too close to the screen!")

            # Comfort alerts
            if temp > 28:
                publish_alert(client, "It's hot — open a window!")
            elif temp < 18:
                publish_alert(client, "It's cold — consider heating.")
            if humid < 40:
                publish_alert(client, "Air is dry — ventilate or hydrate.")

            # Wait before next update
            time.sleep(SLEEP)

    except KeyboardInterrupt:
        pass
    finally:
        # Clean shutdown
        client.loop_stop()
        client.disconnect()

# Run if called directly
if __name__ == "__main__":
    main()
