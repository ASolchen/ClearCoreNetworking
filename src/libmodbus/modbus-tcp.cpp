/*
 * Copyright © 2001-2013 Stéphane Raimbault <stephane.raimbault@gmail.com>
 * Copyright © 2018 Arduino SA. All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "Arduino.h"
#ifndef DEBUG
#define printf(...) {}
#define fprintf(...) {}
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

#if defined(_AIX) && !defined(MSG_DONTWAIT)
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

#if defined(ARDUINO) && defined(__AVR__)
#undef EIO
#define EIO 5

#undef EINVAL
#define EINVAL 22

#undef ENOPROTOOPT
#define ENOPROTOOPT 109

#undef ECONNREFUSED
#define ECONNREFUSED 111

#undef ETIMEDOUT
#define ETIMEDOUT 116

#undef ENOTSUP
#define ENOTSUP 134
#endif

#include "modbus-private.h"
#include "modbus-tcp.h"
#include "modbus-tcp-private.h"

static int _modbus_set_slave(modbus_t *ctx, int slave)
{
    /* Broadcast address is 0 (MODBUS_BROADCAST_ADDRESS) */
    if (slave >= 0 && slave <= 247) {
        ctx->slave = slave;
    } else if (slave == MODBUS_TCP_SLAVE) {
        /* The special value MODBUS_TCP_SLAVE (0xFF) can be used in TCP mode to
         * restore the default value. */
        ctx->slave = slave;
    } else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

/* Builds a TCP request header */
static int _modbus_tcp_build_request_basis(modbus_t *ctx, int function,
                                           int addr, int nb,
                                           uint8_t *req)
{

    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    /* Increase transaction ID */
    if (ctx_tcp->t_id < UINT16_MAX)
        ctx_tcp->t_id++;
    else
        ctx_tcp->t_id = 0;
    req[0] = ctx_tcp->t_id >> 8;
    req[1] = ctx_tcp->t_id & 0x00ff;

    /* Protocol Modbus */
    req[2] = 0;
    req[3] = 0;

    /* Length will be defined later by set_req_length_tcp at offsets 4
       and 5 */

    req[6] = ctx->slave;
    req[7] = function;
    req[8] = addr >> 8;
    req[9] = addr & 0x00ff;
    req[10] = nb >> 8;
    req[11] = nb & 0x00ff;

    return _MODBUS_TCP_PRESET_REQ_LENGTH;
}

/* Builds a TCP response header */
static int _modbus_tcp_build_response_basis(sft_t *sft, uint8_t *rsp)
{
    /* Extract from MODBUS Messaging on TCP/IP Implementation
       Guide V1.0b (page 23/46):
       The transaction identifier is used to associate the future
       response with the request. */
    rsp[0] = sft->t_id >> 8;
    rsp[1] = sft->t_id & 0x00ff;

    /* Protocol Modbus */
    rsp[2] = 0;
    rsp[3] = 0;

    /* Length will be set later by send_msg (4 and 5) */

    /* The slave ID is copied from the indication */
    rsp[6] = sft->slave;
    rsp[7] = sft->function;

    return _MODBUS_TCP_PRESET_RSP_LENGTH;
}


static int _modbus_tcp_prepare_response_tid(const uint8_t *req, int *req_length)
{
    (void)req_length;
    return (req[0] << 8) + req[1];
}

static int _modbus_tcp_send_msg_pre(uint8_t *req, int req_length)
{
    /* Substract the header length to the message length */
    int mbap_length = req_length - 6;

    req[4] = mbap_length >> 8;
    req[5] = mbap_length & 0x00FF;

    return req_length;
}

static ssize_t _modbus_tcp_send(modbus_t *ctx, const uint8_t *req, int req_length)
{

    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    return ctx_tcp->client->write(req, req_length);
}

static int _modbus_tcp_receive(modbus_t *ctx, uint8_t *req) {
    return _modbus_receive_msg(ctx, req, MSG_INDICATION);
}

static ssize_t _modbus_tcp_recv(modbus_t *ctx, uint8_t *rsp, int rsp_length) {
    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;
    return ctx_tcp->client->read(rsp, rsp_length);

}

static int _modbus_tcp_check_integrity(modbus_t *ctx, uint8_t *msg, const int msg_length)
{
    (void)ctx;
    (void)msg;
    return msg_length;
}

static int _modbus_tcp_pre_check_confirmation(modbus_t *ctx, const uint8_t *req,
                                              const uint8_t *rsp, int rsp_length)
{
    (void)rsp_length;
    /* Check transaction ID */
    if (req[0] != rsp[0] || req[1] != rsp[1]) {
        if (ctx->debug) {
            fprintf(stderr, "Invalid transaction ID received 0x%X (not 0x%X)\n",
                    (rsp[0] << 8) + rsp[1], (req[0] << 8) + req[1]);
        }
        errno = EMBBADDATA;
        return -1;
    }

    /* Check protocol ID */
    if (rsp[2] != 0x0 && rsp[3] != 0x0) {
        if (ctx->debug) {
            fprintf(stderr, "Invalid protocol ID received 0x%X (not 0x0)\n",
                    (rsp[2] << 8) + rsp[3]);
        }
        errno = EMBBADDATA;
        return -1;
    }

    return 0;
}


/* Establishes a modbus TCP connection with a Modbus server. */
static int _modbus_tcp_connect(modbus_t *ctx)
{

    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;
    if (!ctx_tcp->client->connect(ctx_tcp->ip, ctx_tcp->port)) {
        return -1;
    }

    return 0;
}


/* Closes the network connection and socket in TCP mode */
static void _modbus_tcp_close(modbus_t *ctx)
{
    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    if(ctx_tcp && ctx_tcp->client) { 
        ctx_tcp->client->stop();
    }
}

static int _modbus_tcp_flush(modbus_t *ctx)
{
    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    while (ctx_tcp->client->available()) {
        ctx_tcp->client->read();
    }

    return 0;
}

/* Listens for any request from one or many modbus masters in TCP */
int modbus_tcp_listen(modbus_t *ctx)
{
    //Clear Core listen here
    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}


int modbus_tcp_accept(modbus_t *ctx, Client* client)

{
    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (client == NULL) {
        errno = EINVAL;
        return -1;
    }

    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    ctx_tcp->client = client;

    return 0;
}



static int _modbus_tcp_select(modbus_t *ctx, fd_set *rset, struct timeval *tv, int length_to_read)
{
    int s_rc;
    (void)rset;

    modbus_tcp_t *ctx_tcp = (modbus_tcp_t*)ctx->backend_data;

    unsigned long wait_time_millis = (tv == NULL) ? 0 : (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
    unsigned long start = millis();

    do {
        s_rc = ctx_tcp->client->available();

        if (s_rc >= length_to_read) {
            break;
        }
    } while ((millis() - start) < wait_time_millis && ctx_tcp->client->connected());

    if (s_rc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    return s_rc;
}

static void _modbus_tcp_free(modbus_t *ctx) {
    free(ctx->backend_data);
    free(ctx);
}

const modbus_backend_t _modbus_tcp_backend = {
    _MODBUS_BACKEND_TYPE_TCP,
    _MODBUS_TCP_HEADER_LENGTH,
    _MODBUS_TCP_CHECKSUM_LENGTH,
    MODBUS_TCP_MAX_ADU_LENGTH,
    _modbus_set_slave,
    _modbus_tcp_build_request_basis,
    _modbus_tcp_build_response_basis,
    _modbus_tcp_prepare_response_tid,
    _modbus_tcp_send_msg_pre,
    _modbus_tcp_send,
    _modbus_tcp_receive,
    _modbus_tcp_recv,
    _modbus_tcp_check_integrity,
    _modbus_tcp_pre_check_confirmation,
    _modbus_tcp_connect,
    _modbus_tcp_close,
    _modbus_tcp_flush,
    _modbus_tcp_select,
    _modbus_tcp_free
};




modbus_t* modbus_new_tcp(Client* client, IPAddress ip_address, int port)
{
    modbus_t *ctx;
    modbus_tcp_t *ctx_tcp;
    ctx = (modbus_t *)malloc(sizeof(modbus_t));
    _modbus_init_common(ctx);

    /* Could be changed after to reach a remote serial Modbus device */
    ctx->slave = MODBUS_TCP_SLAVE;

    ctx->backend = &_modbus_tcp_backend;

    ctx->backend_data = (modbus_tcp_t *)malloc(sizeof(modbus_tcp_t));
    ctx_tcp = (modbus_tcp_t *)ctx->backend_data;

    ctx_tcp->client = client;
    ctx_tcp->ip = ip_address;

    ctx_tcp->port = port;
    ctx_tcp->t_id = 0;

    return ctx;
}

