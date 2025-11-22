# mqttlogger
Logging using mqtt as transport

Work in progress...

# Basic Idea
Use MQTT as a logging mechanism.

A log-receiver subscribes to all log messages and may save them in a
time-stamped log file.

The log-senders send log messages and also subscribe to a control
channel in which the log level can be dynamically changed.


In all this the mosquitto broker and library are used:
https://mosquitto.org/documentation/.


# Features

**Implemented**

`libmql.a` Library to send log messages.

`mql listen` program to receive log messages.

`mql level` program to set log severity level.

`mql count` program to set log severity level for a finite number of sent messages.


**Planned**

`mqld` service program to receive log messages and save to a text file.

Define command/response scheme.


# Actors
## Standard

**MQTT Broker** The MQTT Broker/Server, the basis for the communication. 
Any standard mossquitto broker should do, but only mosquitto tested.


## Logging

**MQL Source**
An MQTT Client using the MQL for logging.

**MQL Receiver**
An MQTT Client receiving MQL Log messages from log sources.

## Control

**MQL Target**
An MQTT Client accepting MQL commands

**MQL Commander**
An MQTT Client sending MQL commands to MQL Targets.




