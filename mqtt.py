import requests
import json
import paho.mqtt.client as paho
import time
from threading import Thread, Event

# MQTT server details
server = 'your-server-ip-address'  
port = 1883

# REST API endpoint
api_url = "http://your-server-ip-address/final/smart_object_api.php"

# Shared storage for received messages
message_queue = []

# Event to stop threads gracefully
stop_event = Event()

# Callback functions
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    # Subscribe to topics upon connection
    client.subscribe("sensor/data/", qos=1)

def on_message(client, userdata, msg):
    print(f"Message received: {msg.topic} -> {msg.payload.decode()}")
    # Store message in the queue
    message_queue.append(json.loads(msg.payload.decode()))

def post_data_periodically():
    while not stop_event.is_set():
        if message_queue:
            try:
                # Copy and clear the queue
                data_to_post = message_queue[:]
                message_queue.clear()
                
                # POST each message in the queue
                for payload in data_to_post:
                    response = requests.post(api_url, json=payload)
                    if response.status_code == 200:
                        print(f"Data posted successfully: {response.text}")
                    else:
                        print(f"Failed to post data. Status code: {response.status_code}")
            except Exception as e:
                print(f"Error posting data: {e}")
        else:
            print("No data to post.")
        
        # Wait for 30 seconds
        time.sleep(3)

# Create MQTT client
client = paho.Client(client_id="MQTT_Client")
client.on_connect = on_connect
client.on_message = on_message

# Connect to the MQTT broker
client.connect(server, port=port)

# Run MQTT loop in a separate thread
mqtt_thread = Thread(target=client.loop_forever)
mqtt_thread.start()

# Start the periodic POSTing thread
post_thread = Thread(target=post_data_periodically)
post_thread.start()

try:
    # Keep the main thread alive
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Stopping...")
    stop_event.set()
    client.disconnect()
    mqtt_thread.join()
    post_thread.join()
    print("Stopped.")
