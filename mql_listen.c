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
 * Last Modified On: Sat Nov 22 17:53:09 2025
 * Update Count    : 105
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



pthread_mutex_t	mtx;
pthread_cond_t	cv;
bool connected = false;



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

unsigned decode_hexdigit(const char* p, size_t len)
{
    unsigned val = ~0U;
    if ( p && (len == 1) ) {
	/* Translate one single hex digit */
	if ( ('0' <= *p) && (*p <= '9') ) {
	    val = *p - '0';
	}
	else if ( ('a' <= *p) && (*p <= 'f') ) {
	    val = *p - 'a' + 10;
	}
	else if ( ('A' <= *p) && (*p <= 'F') ) {
	    val = *p - 'A' + 10;
	}
    }
    return val;
}


#define N_FRAG (8)
void
mql_listen_message_callback(struct mosquitto *pmqc, void *obj,
			    const struct mosquitto_message *msg)
{
    const char*	topic = msg->topic;
    const char*	pload = msg->payload;
    mql_fragment_t frag[N_FRAG];
    char mql_id[ MQL_ID_MAX_LEN + 1];
    size_t mql_id_len = MQL_ID_MAX_LEN;
    int n;
    unsigned tsev;
    const size_t mql_cmd_tag_len = strlen(MQL_CMD_TAG);
    const size_t mql_log_tag_len = strlen(MQL_LOG_TAG);
    
    DD ("%s: \"%s\"\n",__func__, "called");

    n = mql_split(topic,frag,N_FRAG);

    // Ignore control messages: <prefix>/cmd/<id>
    if ( (n == 3) &&
	 (frag[1].len == mql_cmd_tag_len) &&
	 strncmp(MQL_CMD_TAG, frag[1].ptr, mql_cmd_tag_len)  )
	return;

    // Log messages: <prefix>/log/<id>/<severity>
    if ( n != 4					// Must be 4 fragments in topic
	 || frag[1].len != mql_log_tag_len	// Must be a log tag
	 || strncmp(MQL_LOG_TAG, frag[1].ptr, mql_log_tag_len)
	 || frag[3].len != 1 ) {		// Severity must be 1 character
	printf("Error: Malformed topic: \"%s\"! fragments=%d\n\n",topic,n);
	return;
    }


    /* Extract the id and put in a buffer for easy print later. */
    if ( frag[2].len < mql_id_len )
	mql_id_len = frag[2].len;
    strncpy(mql_id,frag[2].ptr,mql_id_len);
    mql_id[ mql_id_len ] = '\0';

    /* Get severity */
    tsev = decode_hexdigit(frag[3].ptr,frag[3].len);

    if ( tsev >= MQL_S_MAX ) {
	printf("Error: Malformed topic: \"%s\"!\n\n",topic);
	return;
    }

    // Only print if severity is below limit.
    if ( tsev <= message_severity )
	printf("%-16s : %x : %-9s : \"%s\"\n",
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

    do {
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
				   false ); /* retain is ON */
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
    } while(0);

    // Do not wait for result.
}

void
mql_count_connect_callback(struct mosquitto *mqc, void *obj, int result)
{
    DD ("%s: \"%s\"\n",__func__, subscribe_topic);
    mql_sub(subscribe_topic);
    set_connected(true);
}


void
mql_count_disconnect_callback(struct mosquitto *mqc, void *obj, int result)
{
    printf("MQTT Disonnected: %d\n", result);
    set_connected(false);
}

void
mql_count_init(const char* host, int port )
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

    mosquitto_connect_callback_set(mqc, mql_count_connect_callback);
    mosquitto_disconnect_callback_set(mqc, mql_count_disconnect_callback);

    i = mosquitto_connect(mqc, host, port, 60);
    if ( i != MOSQ_ERR_SUCCESS) {
	perror("mosquitto_connect: ");
	exit( EXIT_FAILURE );
    }


}

void
mql_command_count(const char* host, int port,
		  const char* topic, unsigned severity, unsigned count)
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
    
    mql_count_init(host,port);

    i = mosquitto_loop_start(mqc);
    if(i != MOSQ_ERR_SUCCESS){
	mosquitto_destroy(mqc);
	fprintf(stderr, "Error: %s\n", mosquitto_strerror(i));
	exit( EXIT_FAILURE );
    }

    do {
	int status;
	char val[MQL_BUFFER_LEN+1];
	int lval;

	/* Do nothing, all happens in the mosquitto thread. */
	if ( opt_d && (n < 5) )
	    printf("loop: %d\n",n++);

	wait_connected();

	lval = snprintf(val, MQL_BUFFER_LEN, "C %x %u",severity,count);
	if ( lval < 5 ) {
	    printf("Bad message len, %d, expected 5 or more!\n",lval);
	    exit( EXIT_FAILURE );
	}
	status = mosquitto_publish(mqc, 0,
				   topic, 
				   lval,
				   val,
				   0,
				   false ); /* retain is ON */
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
    } while(0);

    // Do not wait for result.
}
