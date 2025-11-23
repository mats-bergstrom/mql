/*                               -*- Mode: C -*- 
 * Copyright (C) 2025, Mats Bergstrom
 * 
 * File name       : mql.c
 * Description     : Mqtt Logging
 * 
 * Author          : Mats Bergstrom
 * Created On      : Thu Jul  3 21:27:06 2025
 * 
 * Last Modified By: Mats Bergstrom
 * Last Modified On: Sun Nov 23 17:16:02 2025
 * Update Count    : 68
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "mql.h"

#include <mosquitto.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define STR_MAX (80)
char mqtt_host[ STR_MAX ];

#define MQTT_PORT	(1883)
int mqtt_port = MQTT_PORT;

#define MQL_PREFIX_MAX_LEN	(32)
#define MQL_ID_MAX_LEN		(32)
#define MQL_TOPIC_MAX_LEN	(128)

static char mql_prefix[ MQL_PREFIX_MAX_LEN ];
/* unused: static char mql_id[ MQL_ID_MAX_LEN ]; */
static char mql_topic[ MQL_TOPIC_MAX_LEN ];

int opt_d = 0;
#define DD if(opt_d)printf


#define TARGET_MAX	(80)


void do_help(const char* msg);

    
void
set_prefix(const char* p)
{
    if ( p && *p ) {
	strncpy( mql_prefix, p, MQL_PREFIX_MAX_LEN-1 );
    }
    else {
	char* s = getenv("MQL_PREFIX");
	if ( s ) {
	    strncpy( mql_prefix, s, MQL_PREFIX_MAX_LEN-1 );
	}
	else {
	    strncpy( mql_prefix, "mql", MQL_PREFIX_MAX_LEN-1 );
	}
    }
    DD ("prefix=\"%s\"\n",mql_prefix);
}

void
set_host(const char* h)
{
    if ( h && *h ) {
	strncpy( mqtt_host, h, STR_MAX-1 );
    }
    else {
	char* s = getenv("MQTT_HOST");
	if ( s ) {
	    strncpy( mqtt_host, s, STR_MAX-1 );
	}
	else {
	    strncpy( mqtt_host, "127.0.0.1", STR_MAX-1 );
	}
    }
    DD ("host=\"%s\"\n",mqtt_host);
}



void
set_port(const char* p)
{
    mqtt_port = atoi( p );
}

unsigned
set_severity(const char* s)
{
    unsigned n = MQL_S_MAX;
    if ( s && *s ) {
	if ( !s[1] ) {
	    if ( ('0' <= *s) && (*s <= '9') ) { 
		n = *s - '0';
	    }
	    else if ( ('a' <= *s) && (*s <= 'f') ) {
		n = *s - 'a' + 0x0a;
	    }
	    else {
		do_help("Unrecognised severity number.");
	    }
	}
	else if ( !strcmp("ALL",s) ) {
	    n = MQL_S_MAX-1;
	}
	else if ( !strcmp("FATAL",s) ) {
	    n = MQL_S_FATAL;
	}
	else if ( !strcmp("ERROR",s) ) {
	    n = MQL_S_ERROR;
	}
	else if ( !strcmp("WARNING",s) ) {
	    n = MQL_S_WARNING;
	}
	else if ( !strcmp("INFO",s) ) {
	    n = MQL_S_INFO;
	}
	else {
	    do_help("Unrecognised severity.");
	}
    }
    if ( n >= MQL_S_MAX )
	n = MQL_S_MAX -1;
    return n;
}


void
do_help(const char* msg)
{
    if ( msg )
	printf("Error: %s\n", msg);
    printf(
"mql [-h host] [-p port] [-x prefix] <command> [<args>]\n"
"	Command	Description\n"
"	help	This text.\n"
"	listen	<target> <severity>\n"
"		<target>	ALL or name of target\n"
"		<severity>	[FEWID] or [0-9,a-f] or ALL\n"
"	count	<target> <severity> <count>\n"
"		<target>	ALL or name of target\n"
"		<severity>	[FEWID] or [0-9,a-f] or ALL\n"
"		<count>		No of messages to use severity for\n"
	   );
    exit(0);
}


void mql_command_listen(const char* host, int port,
			const char* topic, unsigned severity);

void
do_listen( int argc, const char** argv )
/* listen [(target|ALL) [severity]]  */
/* topics: <prefix>/log/<target>/<severity> */
{
    const char* target_str = 0;
    const char* severity_str = 0;
    unsigned severity = MQL_S_MAX-1;
    int i;
    
    if ( argc ) {
	target_str = *argv;
	--argc;
	++argv;
	if ( argc ) {
	    severity_str = *argv;
	    --argc;
	    ++argv;

	    if ( argc )
		do_help("Too many arguments to listen command.");

	    severity = set_severity(severity_str);
	}
    }

    DD ("host=\"%s\" port=%d\n",mqtt_host, mqtt_port );
    DD ("target=\"%s\" severity=%u\n", (target_str?target_str:"ALL"), severity);

    if ( !target_str || !*target_str ||
	 !strcmp("ALL",target_str) || !strcmp("*",target_str) ) {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/#", mql_prefix, MQL_LOG_TAG);
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }
    else {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/%s/#", mql_prefix, MQL_LOG_TAG, target_str);
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }
    
    mql_command_listen(mqtt_host,mqtt_port,mql_topic,severity);
    
}


void
mql_command_level(const char* host, int port,
		  const char* topic, unsigned severity);


void
do_level( int argc, const char** argv )
/* level (target|ALL) severity */
{
    const char* target_str = 0;
    const char* severity_str = 0;
    unsigned severity = MQL_S_MAX-1;
    int i;

    if ( argc ) {
	target_str = *argv;
	--argc;
	++argv;
	if ( argc ) {
	    severity_str = *argv;
	    --argc;
	    ++argv;

	    if ( argc )
		do_help("Too many arguments to listen command.");

	    severity = set_severity(severity_str);
	}
    }

    DD ("host=\"%s\" port=%d\n",mqtt_host, mqtt_port );
    DD ("target=\"%s\" severity=%u\n", (target_str?target_str:"ALL"), severity);

    // topic: <prefix> '/' cmd '/' <target>
    if ( !target_str ||
	 !*target_str ||
	 !strcmp("ALL",target_str)
	 || !strcmp("*",target_str) ) {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/%s",mql_prefix, MQL_CMD_TAG, "ALL");
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }
    else {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/%s",mql_prefix, MQL_CMD_TAG, target_str);
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }

    mql_command_level(mqtt_host,mqtt_port,mql_topic,severity);

}


void
mql_command_count(const char* host, int port,
		  const char* topic, unsigned severity, unsigned count);

void
do_count( int argc, const char** argv )
/* count (target|ALL) severity count */
{
    const char* target_str = 0;
    const char* severity_str = 0;
    const char* count_str = 0;
    unsigned severity = MQL_S_MAX-1;
    unsigned long count = 1;
    int i;

    if ( argc ) {
	target_str = *argv;
	--argc;
	++argv;
	if ( argc ) {
	    severity_str = *argv;
	    --argc;
	    ++argv;

	    severity = set_severity(severity_str);

	    if ( argc ) {
		count_str = *argv;
		--argc;
		++argv;
		
		if ( argc )
		    do_help("Too many arguments to count command.");

		count = strtoul(count_str,0,0);
	    }
	}
    }

    DD ("host=\"%s\" port=%d\n",mqtt_host, mqtt_port );
    DD ("target=\"%s\" severity=%u count=%lu\n",
	(target_str?target_str:"ALL"), severity, count);

    
    
    // topic: <prefix> '/' cmd '/' <target>
    if ( !target_str ||
	 !*target_str ||
	 !strcmp("ALL",target_str)
	 || !strcmp("*",target_str) ) {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/%s",mql_prefix, MQL_CMD_TAG, "ALL");
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }
    else {
	i = snprintf(mql_topic,MQL_TOPIC_MAX_LEN,
		     "%s/%s/%s",mql_prefix, MQL_CMD_TAG, target_str);
	if ( !(i<MQL_TOPIC_MAX_LEN) ) abort();
    }

    mql_command_count(mqtt_host,mqtt_port,mql_topic,severity,count);

}



int
main(int argc, const char** argv)
{
    
    setbuf(stdout,0);

    set_host(0);			/* Set default host */
    set_prefix(0);			/* Set default prefix */

    /* Decode arguments. */
    --argc;
    ++argv;
    if ( !argc )
	do_help("Missing command and options.");


    while ( argc ) {
	if ( **argv != '-' )
	    break;

	if ( !strcmp(*argv,"-d") )  {
	    --argc;
	    ++argv;
	    ++opt_d;
	    continue;
	}

	if ( !strcmp(*argv,"-x") )  {
	    --argc;
	    ++argv;
	    if ( !argc || !*argv )
		do_help("Missing argument to -x option");
	    
	    set_prefix( *argv );
	    --argc;
	    ++argv;
	    continue;
	}

	if ( !strcmp(*argv,"-h") )  {
	    --argc;
	    ++argv;
	    if ( !argc || !*argv )
		do_help("Missing argument to -h option");
	    
	    set_host( *argv );
	    --argc;
	    ++argv;
	    continue;
	}

	if ( !strcmp(*argv,"-p") )  {
	    --argc;
	    ++argv;
	    if ( !argc || !*argv )
		do_help("Missing argument to -p option");

	    
	    set_port( *argv );
	    --argc;
	    ++argv;
	    continue;
	}

	printf("Bad option: %s\n",*argv);
	do_help(0);
	break;
    }

    /* Decode commands */
    if ( !argc )
	do_help("Missing command\n");

    if ( !strcmp("listen",*argv) ) {
	--argc;
	++argv;
	do_listen(argc, argv );
    }
    else if ( !strcmp("level", *argv) ) {
	--argc;
	++argv;
	do_level(argc,argv);
    }
    else if ( !strcmp("count", *argv) ) {
	--argc;
	++argv;
	do_count(argc,argv);
    }
    else if ( !strcmp("help", *argv) ) {
	do_help(0);
    }
    else {
	do_help("Unknown command");
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
