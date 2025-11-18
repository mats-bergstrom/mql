/*                               -*- Mode: C -*- 
 * Copyright (C) 2025, Mats Bergstrom
 * 
 * File name       : t-mql.c
 * Description     : Test of mql library
 * 
 * Author          : Mats Bergstrom
 * Created On      : Mon Nov 17 21:36:21 2025
 * 
 * Last Modified By: Mats Bergstrom
 * Last Modified On: Mon Nov 17 22:20:39 2025
 * Update Count    : 18
 * Status          : $State$
 * 
 */


#include "mql.h"

#include <mosquitto.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>


int opt_d = 0;
#define DD if(opt_d)printf

static char my_prefix[ MQL_PREFIX_MAX_LEN ];
static char my_id[ MQL_ID_MAX_LEN ];


#define STR_MAX (80)
char mqtt_host[ STR_MAX ];

#define MQTT_PORT	(1883)
int mqtt_port = MQTT_PORT;

struct mosquitto* mqc = 0;

pthread_mutex_t	mtx;
pthread_cond_t	cv;
bool mq_connected = false;



void
set_prefix(const char* p)
{
    if ( p && *p ) {
	strncpy( my_prefix, p, MQL_PREFIX_MAX_LEN-1 );
    }
    else {
	char* s = getenv("MQL_PREFIX");
	if ( s ) {
	    strncpy( my_prefix, s, MQL_PREFIX_MAX_LEN-1 );
	}
	else {
	    strncpy( my_prefix, "mql", MQL_PREFIX_MAX_LEN-1 );
	}
    }
    DD ("prefix=\"%s\"\n",my_prefix);
}


void
set_id(const char* p)
{
    if ( p && *p ) {
	strncpy( my_id, p, MQL_PREFIX_MAX_LEN-1 );
    }
    else {
	char* s = getenv("MQL_ID");
	if ( s ) {
	    strncpy( my_id, s, MQL_PREFIX_MAX_LEN-1 );
	}
	else {
	    strncpy( my_id, "my-id", MQL_PREFIX_MAX_LEN-1 );
	}
    }
    DD ("id=\"%s\"\n",my_id);
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


void
do_help(const char* msg)
{
    if ( msg )
	printf("Error: %s\n", msg);
    printf(
"mql [-h host] [-p port] [-x prefix] [<command> [<args>]]\n"
"	Command	Description\n"
"	help	This text.\n"
	   );
    exit(0);
}



void
mq_set_connected( bool con )
{
    pthread_mutex_lock( &mtx );
    do {
	mq_connected = con;
	pthread_cond_signal( &cv );
    } while(0);
    pthread_mutex_unlock( &mtx );
}



void
mq_wait_connected()
{
    pthread_mutex_lock( &mtx );
    while ( !mq_connected ) {
	int i;
	i = pthread_cond_wait( &cv, &mtx );
	if ( i ) {
	    perror("Waiting for condvar: ");
	    exit( EXIT_FAILURE );
	}
    } while(0);
    pthread_mutex_unlock( &mtx );
}


void
mq_message_callback(struct mosquitto *pmqc, void *obj,
		    const struct mosquitto_message *msg)
{
    const char*	topic = msg->topic;
    const char*	pload = msg->payload;
    int i;

    DD ("%s: \"%s\" \"%s\"\n",__func__, topic, pload );

    i = mql_message_cb(msg);
    if ( !i ) {
	DD(".. Not MQL message\n");
    }
    else if ( i == -1 ) {
	DD(".. MQL error\n");
    }
    else if (i == 1) {
	DD(".. MQL handled.\n");
    }
    else {
	DD(".. BAD!.\n\n");
    }
}

#if 0
/* unused */
static void
mq_sub(const char* topic)
{
    int i;
    if ( topic ) {
	DD ("%s: topic=%s\n",__func__,topic);
	i = mosquitto_subscribe(mqc, NULL, topic, 0);
	if ( i != MOSQ_ERR_SUCCESS ) {
	    perror("mosquitto_subscribe: ");
	    exit( EXIT_FAILURE );
	}
    }
}
#endif

void
mq_connect_callback(struct mosquitto *mqc, void *obj, int result)
{
    DD ("%s: \n",__func__);
    /*     mql_sub("some/topic"); */

    mql_connect_cb();
    
    /* Release main thread. */
    mq_set_connected( true );
}

void
mq_disconnect_callback(struct mosquitto *mqc, void *obj, int result)
{
    printf("MQTT Disonnected: %d\n", result);
    mq_set_connected( false );
}



void
mq_init(const char* host, int port )
/* Initialise the mosquitto lib */
{
    int i;
    i = mosquitto_lib_init();
    if ( i != MOSQ_ERR_SUCCESS) {
	perror("mosquitto_lib_init: ");
	exit( EXIT_FAILURE );
    }
    
    mqc = mosquitto_new(0, true, 0);
    if ( !mqc ) {
	perror("mosquitto_new: ");
	exit( EXIT_FAILURE );
    }

    mosquitto_connect_callback_set(mqc, mq_connect_callback);
    mosquitto_disconnect_callback_set(mqc, mq_disconnect_callback);
    mosquitto_message_callback_set(mqc, mq_message_callback);

    /* Init the mql library. */
    mql_init(mqc,my_prefix, my_id, MQL_S_INFO );
    
    i = mosquitto_connect(mqc, host, port, 60);
    if ( i != MOSQ_ERR_SUCCESS) {
	perror("mosquitto_connect: ");
	exit( EXIT_FAILURE );
    }


}



void
main_loop()
{
    unsigned n = 0;
    unsigned s = 0;
    /* Wait for us to be connected before we do stuff */
    mq_wait_connected();

    for(;;) {
	int i;
	
	i = mql_log( s, "abc" );
	if (i) {
	    printf("mql_log() failed!\n");
	}
	
	printf("n=%d s=%d\n",n,s);
	++n;
	++s; if ( s >= MQL_S_MAX ) s = 0;
	    
	sleep(1);
    }
}




int
main(int argc, const char** argv)
{
    int i;
    
    setbuf(stdout,0);

    set_host(0);			/* Set default host */
    set_prefix(0);			/* Set default prefix */

    set_id(0);
    
    pthread_mutex_init( &mtx, 0 );
    pthread_cond_init( &cv, 0 );
    mq_connected = false;

    
    /* Decode arguments. */
    --argc;
    ++argv;



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



    mq_init(mqtt_host,mqtt_port);

    i = mosquitto_loop_start(mqc);
    if(i != MOSQ_ERR_SUCCESS){
	mosquitto_destroy(mqc);
	fprintf(stderr, "Error: %s\n", mosquitto_strerror(i));
	exit( EXIT_FAILURE );
    }

    main_loop();
    
    
    exit( EXIT_SUCCESS );
}
