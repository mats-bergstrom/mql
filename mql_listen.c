/*                               -*- Mode: C -*- 
 * Copyright (C) 2025, Mats Bergstrom
 * 
 * File name       : mql_listen.c
 * Description     : Listen+level command
 * 
 * Author          : Mats Bergstrom
 * Created On      : Sun Jul  6 09:55:40 2025
 * 
 * Last Modified By: Mats Bergstrom
 * Last Modified On: Mon Nov 17 18:48:01 2025
 * Update Count    : 87
 */


#include "mql.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __linux__
#include <sched.h>
#define pthread_yield  sched_yield
#endif

extern int opt_d;
#define DD if(opt_d)printf

struct mosquitto* mqc = 0;
unsigned message_severity = MQL_S_MAX-1;
char subscribe_topic[ MQL_STRING_MAX ];

#define MQL_ID_MAX_LEN		(32)
static char mql_id[ MQL_ID_MAX_LEN ];


pthread_mutex_t	mtx;
pthread_cond_t	cv;
bool connected = false;


unsigned
topic_severity(const char* s)
/* <prefix> '/' <id> '/' <severity>  */
/* Extract the severity from the topic string. One single hex digit */
/* Returns severity or MQL_S_MAX to indicate error. */
/* Puts <id> in mql_id[] */
{
    unsigned severity = MQL_S_MAX;	/*  */
    const char* p = 0;
    char* q = mql_id;
    unsigned n = 0;

    *q = '\0';

    do {
	/* Jump over first '/' */
	while ( s && *s && !p ) {
	    if ( *s == '/' )
		p = s;
	    ++s;
	}

	/* Step to next '/' and copy to mql_id[] */
	while ( s && *s ) {
	    if ( *s == '/' ) {
		p = s;
		++s;
		break;
	    }
	    if ( n < (MQL_ID_MAX_LEN-1) ) {
		*q = *s;
		++s;
		++q;
		*q = '\0';
		++n;
	    }
	    else {
		/* Too long id, ignore end of it */
		++s;
	    }
	}
	

	if ( !p )
	    break;			/* No '/' found : Error */

	if ( !n )			/* No id found : Error */
	    break;
	
	++p;				/* Jump over '/' */
	if ( !*p )
	    break;

	/* Translate one single hex digit */
	if ( ('0' <= *p) && (*p <= '9') ) {
	    severity = *p - '0';
	}
	else if ( ('a' <= *p) && (*p <= 'f') ) {
	    severity = *p - 'a' + 10;
	}
	else if ( ('A' <= *p) && (*p <= 'F') ) {
	    severity = *p - 'A' + 10;
	}
	else {
	    /* Not a hex digit : Error */
	    break;
	}

	/* Not the last character :  Error */
	++p;
	if ( *p )
	    severity = MQL_S_MAX;
    } while(0);

    return severity;
}

static const char* mql_sev_name[MQL_S_MAX] = {
    "FATAL",
    "ERROR",
    "WARNING",
    "WARNING_1",
    "INFO",
    "INFO_1",
    "INFO_2",
    "INFO_3",
    "DEBUG",
    "DEBUG_1",
    "DEBUG_2",
    "DEBUG_3",
    "DEBUG_4",
    "DEBUG_5",
    "DEBUG_6",
    "DEBUG_7"
};


int
ends_in(const char* topic, const char* end)
{
    const char* s = strstr(topic,end);
    int l = strlen(end);
    if ( !s )
	return 0;
    if ( *(s+l) != '\0' )
	return 0;
    return 1;
    
}


void
mql_listen_message_callback(struct mosquitto *pmqc, void *obj,
		    const struct mosquitto_message *msg)
{
    const char*	topic = msg->topic;
    const char*	pload = msg->payload;
    unsigned	tsev = topic_severity(topic);

    DD ("%s: \"%s\"\n",__func__, "called");

    if ( tsev >= MQL_S_MAX ) {

	if ( ends_in(topic,"control") ) return;

	printf("Error: Malformed topic: \"%s\"!\n\n",topic);
	return;
    }

    if ( tsev <= message_severity )
	printf("%-16s : %u : %-9s : \"%s\"\n",
	       mql_id, tsev, mql_sev_name[tsev],pload);
}


static void
mql_sub(const char* topic)
{
    int i;
    DD ("%s: topic=%s\n",__func__,topic);
    i = mosquitto_subscribe(mqc, NULL, topic, 0);
    if ( i != MOSQ_ERR_SUCCESS ) {
	perror("mosquitto_subscribe: ");
	exit( EXIT_FAILURE );
    }
}

void
mql_listen_connect_callback(struct mosquitto *mqc, void *obj, int result)
{
    DD ("%s: \"%s\"\n",__func__, subscribe_topic);
    mql_sub(subscribe_topic);
}

void
mql_listen_disconnect_callback(struct mosquitto *mqc, void *obj, int result)
{
    printf("MQTT Disonnected: %d\n", result);
    exit( EXIT_FAILURE );
}

void
mql_listen_init(const char* host, int port )
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

    mosquitto_connect_callback_set(mqc, mql_listen_connect_callback);
    mosquitto_disconnect_callback_set(mqc, mql_listen_disconnect_callback);
    mosquitto_message_callback_set(mqc, mql_listen_message_callback);

    i = mosquitto_connect(mqc, host, port, 60);
    if ( i != MOSQ_ERR_SUCCESS) {
	perror("mosquitto_connect: ");
	exit( EXIT_FAILURE );
    }


}

void
mql_command_listen(const char* host, int port,
		   const char* topic, unsigned severity)
{
    int i;
    unsigned int n = 0;

    if ( !topic ) abort();
    if ( !*topic ) abort();
    
    message_severity = severity;
    strncpy(subscribe_topic,topic,MQL_STRING_MAX-1);
    
    mql_listen_init(host,port);

    i = mosquitto_loop_start(mqc);
    if(i != MOSQ_ERR_SUCCESS){
	mosquitto_destroy(mqc);
	fprintf(stderr, "Error: %s\n", mosquitto_strerror(i));
	exit( EXIT_FAILURE );
    }

    for(;;) {
	/* Do nothing, all happens in the mosquitto thread. */
	if ( opt_d && (n < 5) )
	    printf("loop: %d\n",n++);
	sleep(1);
    }
}


void
set_connected( bool con )
{
    pthread_mutex_lock( &mtx );
    do {
	connected = con;
	pthread_cond_signal( &cv );
    } while(0);
    pthread_mutex_unlock( &mtx );

}

void
wait_connected()
{
    pthread_mutex_lock( &mtx );
    while ( !connected ) {
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
mql_level_connect_callback(struct mosquitto *mqc, void *obj, int result)
{
    DD ("%s: \"%s\"\n",__func__, subscribe_topic);
    mql_sub(subscribe_topic);
    set_connected(true);
}


void
mql_level_disconnect_callback(struct mosquitto *mqc, void *obj, int result)
{
    printf("MQTT Disonnected: %d\n", result);
    set_connected(false);
}

void
mql_level_init(const char* host, int port )
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

    mosquitto_connect_callback_set(mqc, mql_level_connect_callback);
    mosquitto_disconnect_callback_set(mqc, mql_level_disconnect_callback);

    i = mosquitto_connect(mqc, host, port, 60);
    if ( i != MOSQ_ERR_SUCCESS) {
	perror("mosquitto_connect: ");
	exit( EXIT_FAILURE );
    }


}

void
mql_command_level(const char* host, int port,
		  const char* topic, unsigned severity)
{
    int i;
    unsigned int n = 0;

    pthread_mutex_init( &mtx, 0 );
    pthread_cond_init( &cv, 0 );
    connected = false;
    
    if ( !topic ) abort();
    if ( !*topic ) abort();
    
    message_severity = severity;
    strncpy(subscribe_topic,topic,MQL_STRING_MAX-1);
    
    mql_level_init(host,port);

    i = mosquitto_loop_start(mqc);
    if(i != MOSQ_ERR_SUCCESS){
	mosquitto_destroy(mqc);
	fprintf(stderr, "Error: %s\n", mosquitto_strerror(i));
	exit( EXIT_FAILURE );
    }

    for(;;) {
	int status;
	char val[MQL_BUFFER_LEN+1];
	int lval;

	/* Do nothing, all happens in the mosquitto thread. */
	if ( opt_d && (n < 5) )
	    printf("loop: %d\n",n++);

	wait_connected();

	lval = snprintf(val, MQL_BUFFER_LEN, "L %x",severity);
	if ( lval != 3 ) {
	    printf("Bad message len, %d, expected 3!\n",lval);
	    exit( EXIT_FAILURE );
	}
	status = mosquitto_publish(mqc, 0,
				   topic, 
				   lval,
				   val,
				   0,
				   true ); /* retain is ON */
	if ( status != MOSQ_ERR_SUCCESS ) {
	    printf("mosquitto_publish FAILED: %d\n",status);
	    exit( EXIT_FAILURE );
	}

	i = pthread_yield();
	if ( i )
	    perror("pthread_yield: ");

	i = pthread_yield();
	if ( i )
	    perror("pthread_yield: ");

	i = pthread_yield();
	if ( i )
	    perror("pthread_yield: ");

	break;
    }
}
