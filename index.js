const mqtt = require('mqtt');
const express = require("express");

const app = express()

// MQTT broker address and port
const MQTT_BROKER = '192.168.244.82';
const MQTT_PORT = 1883;

// MQTT topic to subscribe to
const MQTT_TOPIC = 'test';

// Create MQTT client instance
const client = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`);

const temperatures = [];
const humidities = []

// Callback function to handle incoming MQTT messages
client.on('message', function(topic, message) {
    let res = JSON.stringify(message.toString());
    console.log(res["humidity"])
    console.log(res);
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

