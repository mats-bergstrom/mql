/*                               -*- Mode: C -*- 
 * Copyright (C) 2025, Mats Bergstrom
 * 
 * File name       : mql.h
 * Description     : Mqtt Logging
 * 
 * Author          : Mats Bergstrom
 * Created On      : Sun Jun 29 10:59:37 2025
 * 
 * Last Modified By: Mats Bergstrom
 * Last Modified On: Tue Nov 18 18:00:16 2025
 * Update Count    : 21
 */

#ifndef __MQL_H__
#define __MQL_H__ (1)

/*
 * Topics:
 * log-topics:	<prefix>/<unit-id>/<severity>
 * log-message:	<string>
 *	<prefix>	Common prefix sxtring for all topics, eg "mylog"
 *	<unit-id>	Id string identifying the unit.
 *	<severity>	hex coded number 0..f
 *	<string>	User defined string, length < 256.
 *
 * control-topics:	<prefix>/{<unit-id>|ALL}/control
 * control-message:	<command><space><arg0>[<space><arg1>]
 *	<command>	L	<arg0>=hex coded level
 *		Change log level to <arg0>
 *			C	<arg0>=level <arg1>=count
 *		Change log level to <arg0> for <arg1> messages.
 */

#include <mosquitto.h>

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


// Max size of message strings
#define MQL_STRING_MAX	(256)

// Misc Sizes.
#define MQL_PREFIX_MAX_LEN	(32)
#define MQL_ID_MAX_LEN		(32)
#define MQL_TOPIC_MAX_LEN	(128)
#define MQL_BUFFER_LEN		MQL_STRING_MAX


// Initialise
//	mqc		mqtt handle
//	prefix		General mqtt topic prefix
//	id		Id string of calling process
//	lvl		Start log level.
//	RETURNS	0	OK
//		-1	Error
// Call after mosquitto_new() and before mosquitto_connect().
int mql_init(struct mosquitto* mqc,
	     const char* prefix, const char* id, unsigned lvl);

// Use in MQTT connect callback.  Call to subscribe to control topics.
//	RETURNS	0	OK
//		-1	Error
int mql_connect_cb();

// Use in MQTT message callback.
//	msg		mqtt message 
//	RETURNS	0	OK, was not mql related
//		-1	Error.
//		1	OK, was mql related.
int mql_message_cb(const struct mosquitto_message *msg);


// Use to send log messages
//	severity	Severity of message, MQL_S_FATAL..MQL_S_MAX-1
//	msg_id		Message id (user defined value)
//	string		Message string (user defined value)
//	RETURNS	0	OK
//		-1	Error
int mql_log(unsigned severity, const char* string);
int mql_logf(unsigned severity, const char* format, ... );

// Set maximum severity level to emit.
int mql_set_level(unsigned severity);

// Set maximum severity level to emit for count emissions
int mql_set_level_counted(unsigned severity, unsigned count);


/* Help Functions */
typedef struct {
    const char* ptr;
    size_t	len;
} mql_fragment_t;

/* Fills in the frag_array to point to start of each fragment and length */
/* of each fragment. */
/* Returns -1 for error, or number of fragments decoded */
int mql_split(const char* topic,
	      mql_fragment_t* frag_array, size_t frag_array_len );

#endif
