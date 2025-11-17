/*                               -*- Mode: C -*- 
 * Copyright (C) 2025, Mats Bergstrom
 * 
 * File name       : mqllib.c
 * Description     : Mqtt Logging Library
 * 
 * Author          : Mats Bergstrom
 * Created On      : Thu Jul  3 21:27:06 2025
 * 
 * Last Modified By: Mats Bergstrom
 * Last Modified On: Mon Nov 17 17:39:30 2025
 * Update Count    : 36
 */


/* NOTE: Currently only using mosquitto */

#include "mql.h"

#include <mosquitto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define DD if(1)printf

#ifndef MQL_DEFAULT_LEVEL
#define MQL_DEFAULT_LEVEL MQL_S_WARNING
#endif



static char mql_prefix[ MQL_PREFIX_MAX_LEN ];
static char mql_id[ MQL_ID_MAX_LEN ];

static char mql_log_topic[ MQL_S_MAX ][ MQL_TOPIC_MAX_LEN ];
static char mql_cmd_topic[ MQL_TOPIC_MAX_LEN ];
static char mql_cmd_topic_all[ MQL_TOPIC_MAX_LEN ];

static char mql_buffer[ MQL_BUFFER_LEN ];

static unsigned mql_level = MQL_S_INFO;
static unsigned mql_clevel = MQL_S_INFO;
static unsigned mql_count = 0;

static struct mosquitto* mql_mqc = 0;

int
mql_init(struct mosquitto* mqc,
	 const char* prefix, const char* id, unsigned lvl)
{
    unsigned l;
    int i;

    if ( !mqc ) abort();
    if ( !prefix ) abort();
    if ( !*prefix ) abort();
    if ( !id ) abort();
    if ( !*id ) abort();
    if ( lvl >= MQL_S_MAX ) abort();

    mql_mqc = mqc;
    
    strncpy( mql_prefix, prefix, MQL_PREFIX_MAX_LEN-1);
    strncpy( mql_id, id, MQL_ID_MAX_LEN-1 );

    for ( l = 0; l < MQL_S_MAX; ++l ) {
	i = snprintf( mql_log_topic[l], MQL_TOPIC_MAX_LEN,
		      "%s/%s/%x", mql_prefix, mql_id, l );
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
	
    }

    i = snprintf( mql_cmd_topic, MQL_TOPIC_MAX_LEN,
		  "%s/%s/control", mql_prefix, mql_id);
    if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();

    i = snprintf( mql_cmd_topic_all, MQL_TOPIC_MAX_LEN,
		  "%s/ALL/control", mql_prefix);
    if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();

    mql_level = lvl;
    mql_clevel = lvl;
    mql_count = 0;
    
    return 0;
}


// Use in MQTT connect callback.
int
mql_connect_cb()
{
    mosquitto_subscribe(mql_mqc, NULL, mql_cmd_topic, 0);
    mosquitto_subscribe(mql_mqc, NULL, mql_cmd_topic_all, 0);
    return 0;
}


#define MQL_LEVEL_COMMAND	'L'
#define MQL_COUNT_COMMAND	'C'

static unsigned
mql_decode_lvl(const char* s, unsigned* lvl_ptr )
{
    unsigned lvl = 0;
    unsigned n = 0;
    if ( *s == ' ' ) {
	++s;
	++n;

	if ( ('0' <= *s) && (*s <= '9')) {
	    lvl = (*s - '0');
	    ++s;
	    ++n;
	}
	else if ( ('a' <= *s) && (*s <= 'f') ) {
	    lvl = (*s - 'a') + 0x0a;
	    ++s;
	    ++n;
	}
	else {
	    n = 0;
	}
    }
    if ( n )
	*lvl_ptr = lvl;
    return n;
}


static unsigned
mql_decode_count(const char* s, unsigned* count_ptr )
{
    unsigned count = 0;
    unsigned n = 0;
    if ( *s == ' ' ) {
	++s;
	++n;

	if ( *s ) {
	    while ( *s ) {
		if ( ('0' <= *s) && (*s <= '9')) {
		    count *= 10;
		    count += (*s - '0');
		    ++s;
		    ++n;
		}
		else {
		    n = 0;
		    break;
		}
	    }
	}
	else {
	    n = 0;
	}
	
    }
    if ( n )
	*count_ptr = count;
    return n;
}


static int
mql_do_command(const char* cmd)
{
    unsigned l = 0;
    DD ("Command \"%s\"",cmd);
    if ( *cmd == MQL_LEVEL_COMMAND ) {
	unsigned lvl = 0;
	++cmd;
	l = mql_decode_lvl(cmd,&lvl);
	if ( l > 0 ) {
	    mql_level = lvl;
	    l = 1;
	}
	else {
	    l = -1;
	}
    }
    else if ( *cmd == MQL_COUNT_COMMAND ) {
	unsigned lvl = 0;
	unsigned count = 0;
	++cmd;
	l = mql_decode_lvl(cmd,&lvl);
	if ( l > 0 ) {
	    cmd += l;
	    l = mql_decode_count(cmd,&count);
	    if ( l > 0 ) {
		mql_clevel = lvl;
		mql_count = count;
		l = 1;
	    }
	    else {
		l = -1;
	    }
	}
	else {
	    l = -1;
	}
    }
    return l;
}


// Use in MQTT message callback.
int
mql_message_cb(const struct mosquitto_message *msg)
{
    int i = 0;
    const char*    topic = msg->topic;
    const char*    pload = msg->payload;
    if ( !mql_mqc ) abort();
    if ( !strncmp(topic,mql_cmd_topic,MQL_TOPIC_MAX_LEN) ||
	 !strncmp(topic,mql_cmd_topic_all,MQL_TOPIC_MAX_LEN) ) {
	i = mql_do_command(pload);
    }
    return i;
}



// Set severity level
int
mql_set_level(unsigned severity)
{
    if ( severity >= MQL_S_MAX )
	return -1;
    mql_level = severity;
    return 0;
}

// Set severity level, counted
int
mql_set_level_counted(unsigned severity, unsigned count)
{
    if ( severity >= MQL_S_MAX )
	return -1;
    if ( !count )
	return -1;
    mql_clevel = severity;
    mql_count = count;
    return 0;
}



// Use to send log messages
int
mql_log(unsigned severity, const char* string)
{
    int n = 0;
    int status;
    const char* topic;
    
    if ( !mql_mqc ) abort();

    if ( mql_count ) {
	if  (severity > mql_clevel)
	    return 0;
	--mql_count;
    }
    if ( severity > mql_level )
	return 0;

    if ( !string )
	return -1;

    n = strlen( string);

    topic = mql_log_topic[severity&0x0f];

    DD ("Topic: %u \"%s\"\nMessage: %d \"%s\"\n", severity, topic , n, string);
    
    status = mosquitto_publish(mql_mqc, 0,
			       topic,
			       n,
			       string,
			       0,
			       false );			/* retain is OFF */
    if ( status != MOSQ_ERR_SUCCESS )
	return -1;

    return 0;
}


int
mql_logf(unsigned severity, const char* format, ... )
{
    va_list ap;
    int i;
    
    va_start( ap, format );
    i = vsnprintf(mql_buffer, MQL_BUFFER_LEN-1,format,ap);
    va_end(ap);

    if ( i > 0 )
	mql_log(severity,mql_buffer);

    return 0;
}
