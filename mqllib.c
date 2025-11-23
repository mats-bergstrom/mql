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
 * Last Modified On: Sun Nov 23 17:36:45 2025
 * Update Count    : 59
 */


/* NOTE: Currently only using mosquitto */

#include "mql.h"

#include <mosquitto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static const int opt_dd = 0;

#define DD if(opt_dd)printf

#ifndef MQL_DEFAULT_LEVEL
#define MQL_DEFAULT_LEVEL MQL_S_WARNING
#endif


static const char mql_log_tag[] = MQL_LOG_TAG;
static const char mql_cmd_tag[] = MQL_CMD_TAG;
//static const char mql_rsp_tag[] = MQL_RSP_TAG;

static char mql_prefix[ MQL_PREFIX_MAX_LEN ];
static char mql_id[ MQL_ID_MAX_LEN ];
static const char mql_id_ALL[] = "ALL";

static char mql_log_topic[ MQL_S_MAX ][ MQL_TOPIC_MAX_LEN ];
static char mql_cmd_topic[ MQL_TOPIC_MAX_LEN ];
static char mql_cmd_topic_all[ MQL_TOPIC_MAX_LEN ];
//static char mql_rsp_topic[ MQL_TOPIC_MAX_LEN ];

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

    DD("prefix=\"%s\" id=\"%s\" lvl=%d\n", prefix,id,lvl);
    
    mql_mqc = mqc;
    
    strncpy( mql_prefix, prefix, MQL_PREFIX_MAX_LEN-1);
    strncpy( mql_id, id, MQL_ID_MAX_LEN-1 );

    // Log Topics: <prefix> '/' <log-tag> '/' <id> '/' <severity>
    for ( l = 0; l < MQL_S_MAX; ++l ) {
	i = snprintf( mql_log_topic[l], MQL_TOPIC_MAX_LEN,
		      "%s/%s/%s/%x", mql_prefix, mql_log_tag, mql_id, l );

	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
	DD(".. log_topic[%x]=\"%s\"\n",l,mql_log_topic[l]);
    }

    // Command Topics: <prefix> '/' <cmd-tag> '/' <id>
    i = snprintf( mql_cmd_topic, MQL_TOPIC_MAX_LEN,
		  "%s/%s/%s", mql_prefix, mql_cmd_tag, mql_id);
    if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    DD(".. cmd_topic=\"%s\"\n",mql_cmd_topic);

    // Broadcast Command Topics: <prefix> '/' <cmd-tag> '/' 'ALL'
    i = snprintf( mql_cmd_topic_all, MQL_TOPIC_MAX_LEN,
		  "%s/%s/%s", mql_prefix, mql_cmd_tag, mql_id_ALL );
    if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    DD(".. cmd_topic_all=\"%s\"\n",mql_cmd_topic_all);

    mql_level = lvl;
    mql_clevel = lvl;
    mql_count = 0;
    
    return 0;
}


// Use in MQTT connect callback.
int
mql_connect_cb(struct mosquitto* mqc)
{
    mosquitto_subscribe(mqc, NULL, mql_cmd_topic, 0);
    mosquitto_subscribe(mqc, NULL, mql_cmd_topic_all, 0);
    return 0;
}


#define MQL_LEVEL_COMMAND	'L'
#define MQL_COUNT_COMMAND	'C'

// Decode a single hex digit to a number.
// Returns: 1 on success, 0 on failure. 
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
	else if ( ('A' <= *s) && (*s <= 'F') ) {
	    lvl = (*s - 'A') + 0x0a;
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


// Decodes a decimal string to a number.
// Return 0 on failure, >0 number of characters used.
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
mql_do_command(struct mosquitto* mqc, const char* cmd)
{
    unsigned l = 0;
    DD ("Command \"%s\"\n",cmd);
    if ( *cmd == MQL_LEVEL_COMMAND ) {
	unsigned lvl = 0;
	++cmd;
	l = mql_decode_lvl(cmd,&lvl);
	DD ("New level = %d was %d, l = %d\n",lvl, mql_level,l);
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
	    DD ("New clevel = %d / %d, count=%d, l = %d\n\n",
		lvl, mql_level,count,l);
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
mql_message_cb(struct mosquitto* mqc,const struct mosquitto_message *msg)
{
    int i = 0;
    const char*    topic = msg->topic;
    const char*    pload = msg->payload;
    if ( !mql_mqc ) abort();
    if ( !strncmp(topic,mql_cmd_topic,MQL_TOPIC_MAX_LEN) ||
	 !strncmp(topic,mql_cmd_topic_all,MQL_TOPIC_MAX_LEN) ) {
	i = mql_do_command(mqc,pload);
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

    DD("mql_log(%x/%x/%d(%u),\"%s\")\n",
       severity,mql_level,mql_clevel,mql_count,string);
    if ( !mql_mqc ) abort();

    if ( mql_count ) {
	if  (severity > mql_clevel)
	    return 0;
    }
    else if ( severity > mql_level ) {
	return 0;
    }

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

    if ( mql_count )
	--mql_count;
    
    if ( status != MOSQ_ERR_SUCCESS )
	return -1;

    return 0;
}


unsigned
mql_get_level()
{
    unsigned l = (mql_count ? mql_clevel : mql_level );
    return l;	
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


int
mql_split(const char* topic, mql_fragment_t* frag_array, unsigned frag_array_len )
{
    unsigned fragments = 0;
    const char* p = topic;
    const char* ptr=topic;
    unsigned	len=0;
    if ( !topic || !frag_array || !frag_array_len ) return -1;
    for(;;) {
	if ( !*p ) {
	    /* at end of topic */
	    if ( fragments < frag_array_len ) {
		frag_array[ fragments ].ptr = ptr;
		frag_array[ fragments ].len = len;
		++fragments;
	    }
	    break;
	}
	if ( *p == '/' ) {
	    if ( fragments < frag_array_len ) {
		frag_array[ fragments ].ptr = ptr;
		frag_array[ fragments ].len = len;
		++fragments;

		++p;
		ptr = p;
		len = 0;
	    }
	    else {
		break;
	    }
	}
	else {
	    ++p;
	    ++len;
	}
    }
    return fragments;
}
