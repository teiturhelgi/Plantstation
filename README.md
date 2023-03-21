# Plantstation
IoT Plant Monitoring System - Final project for Programming of Embedded Wireless Systems and Sensors course at DTU. 


We have created a prototype for an automated greenhouse system. It can be set up anywhere on the planet, 
in particular in places with terrain not suited for natural growth. The system uses sensors to monitor 
the conditions inside the green house, and adjusts lighting, humidity and soil moisture levels accordingly. 
In this prototype, a scaled-down version of the automatic greenhouse system was created, replacing the 
grow light, water pump and humidifier with LED’s.


The system is made of two ESP32s, a Raspberry Pi, 3 LEDs, an OLED display and 3 sensors. 
The first ESP32 is connected to three sensors measuring temperature, humidity, soil moisture and light. 
Using the MQTT protocol, the ESP32 publishes its data to a certain topic. The message is sent through a 
broker on the Raspberry Pi using node-red. The Raspberry Pi manages the messages coming in, and sends the
data forward to a ThingSpeak dashboard, via HTTP, as well as to the subscribers, in this case the other 
ESP32 device. The second ESP32, which subscribes to the sensor data from the sensor ESP32, gets the data 
from the Raspberry Pi using MQTT. Using the sensor data, the second ESP32 evaluates whether or not to 
turn on the warning lights, for either water, humidity or brightness. It also takes the data and displays 
it on an OLED display to the user.



app_main.c contains code for Sensor control ESP32 device.

app_main2.c contains code for Output control (Display) ESP32.



Attached is a poster with the system overview:
[poster.pdf](https://github.com/teiturhelgi/Plantstation/files/11026801/poster.pdf)

Full report:
[Final_assignment_PEWS-2.pdf](https://github.com/teiturhelgi/Plantstation/files/11027253/Final_assignment_PEWS-2.pdf)

