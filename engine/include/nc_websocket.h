/*
 * nc_websocket.h — WebSocket support for NC (server + client).
 */

#ifndef NC_WEBSOCKET_H
#define NC_WEBSOCKET_H

#include <stdbool.h>
#include <stdint.h>
#include "nc_platform.h"

/* Server-side WebSocket */
int  nc_ws_send(nc_socket_t fd, const char *message);
int  nc_ws_read(nc_socket_t fd, char *buf, int buf_size);
void nc_ws_broadcast(const char *message);
void nc_ws_compute_accept(const char *client_key, char *out, int out_size);

/* Connection management */
int  nc_ws_add_connection(nc_socket_t fd, const char *path);
void nc_ws_remove_connection(int id);

/* HTTP Upgrade handler — parses Sec-WebSocket-Key, sends 101, adds to pool */
int  nc_ws_handle_upgrade(int client_fd, const char *request_headers);

/* Per-connection outbound message queue (thread-safe via ws_mutex) */
int  nc_ws_queue_message(int conn_id, const char *msg, int len);
int  nc_ws_flush_queue(int conn_id);

/* Ping/Pong keepalive */
int  nc_ws_ping(int conn_id);

/* Connection groups/rooms for pub/sub */
int  nc_ws_join_room(int conn_id, const char *room);
int  nc_ws_leave_room(int conn_id, const char *room);
int  nc_ws_broadcast_room(const char *room, const char *msg, int len);

/* Client-side WebSocket — connect to external ws:// endpoints */
nc_socket_t nc_ws_client_connect(const char *url);
int         nc_ws_client_send(nc_socket_t fd, const char *message);
void        nc_ws_client_close(nc_socket_t fd);

#endif /* NC_WEBSOCKET_H */
