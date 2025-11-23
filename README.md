# mqttlogger
Logging using mqtt as transport

Work in progress...

# Basic Idea
Use MQTT as a logging mechanism.

The log-sources send log messages.
Logging is done using a simple string interface or printf-like calls:
```
int mql_log(unsigned severity, const char* string);
int mql_logf(unsigned severity, const char* format, ... );
```

A simple log-receiver program (`mql`) subscribes to log messages and prints to stdout:
```
mql listen ALL ALL
```

The log-senders also subscibe to receive control messages to allow for dynamic change of the log severity level.
The `mql` program can be use to send those control messages:
```
mql level DEBUG
```

The mosquitto broker and library are used:
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

- **MQTT Broker** 
  - The MQTT Broker/Server, the basis for the communication. 
Any standard mossquitto broker should do, but only mosquitto tested.

- **MQTT Client**
  - An MQTT client connecting to the MQTT Broker.

## Logging

- **MQL Source**
  - An MQTT Client using the MQL for logging.

- **MQL Receiver**
  - An MQTT Client receiving MQL Log messages from log sources.

## Control

- **MQL Target**
  - An MQTT Client accepting MQL commands

- **MQL Commander**
  - An MQTT Client sending MQL commands to MQL Targets.


# Topics and Messages
## Logging

|    | Composition | Example |
| --- | --- | --- |
| Topic | `<prefix> / log / <id> / <severity>` | `mql/log/testapp/3` |
| Message | `<log-id> <space> <text>` | `404 Not found.` |

For log topics:
| Component | Type | Default | Description |
| --- | --- | --- | --- |
| `<prefix>` | String | `mql` | Prefix for all MQL messages. |
| `log` | String | `log` | Indicates an MQL log message. |
| `<id>`  | String | n/a | Name of the log source. |
| `<severity>` | hex digit | n/a | Severity encoded in hexadecimal |

For log messages:
| Component | Type | Default | Description |
| --- | --- | --- | --- |
| `<log-id>` | decimal | n/a | Decimal number of log message. |
| `<text>` | String | n/a | Log message. |

The formatting of log messages is not enforced.

Severity levels:
```
#define MQL_S_FATAL	(0)
#define MQL_S_ERROR	(1)
#define MQL_S_WARNING	(2)
#define MQL_S_INFO	(4)
#define MQL_S_DEBUG	(8)
#define MQL_S_MAX	(16)

#define MQL_S_WARNING_0	(2+0)
#define MQL_S_WARNING_1	(2+1)
#define MQL_S_INFO_0	(4+0)
#define MQL_S_INFO_1	(4+1)
#define MQL_S_INFO_2	(4+2)
#define MQL_S_INFO_3	(4+3)
#define MQL_S_DEBUG_0	(8+0)
#define MQL_S_DEBUG_1	(8+1)
#define MQL_S_DEBUG_2	(8+2)
#define MQL_S_DEBUG_3	(8+3)
#define MQL_S_DEBUG_4	(8+4)
#define MQL_S_DEBUG_5	(8+5)
#define MQL_S_DEBUG_6	(8+6)
#define MQL_S_DEBUG_7	(8+7)
```

## Control

## Response
*TBD*




