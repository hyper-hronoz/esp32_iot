const mqtt = require('mqtt');
const express = require("express");

const app = express()

// MQTT broker address and port
const MQTT_BROKER = '192.168.0.147';
const MQTT_PORT = 1883;

// MQTT topic to subscribe to
const MQTT_TOPIC = 'test';

// Create MQTT client instance
const client = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`);

// Callback function to handle incoming MQTT messages
const current_msg = "";
client.on('message', function(topic, message) {
    current_msg = msg;
    console.log(`Received message: ${message.toString()}`);
});

app.get("/", (req, res) => {
    res.send({msg: current_msg});
})

// Connect to MQTT broker and subscribe to the topic
client.on('connect', function() {
    console.log('Connected to MQTT broker');
    client.subscribe(MQTT_TOPIC, function(err) {
        if (err) {
            console.error('Error subscribing to topic:', err);
        } else {
            console.log(`Subscribed to topic '${MQTT_TOPIC}'`);
        }
    });
});

// Handle errors
client.on('error', function(err) {
    console.error('Error:', err);
});

