/**
 * sockets plugin version 1.0.0 by Manuel Schnitzer
 *
 * https://github.com/mschnitzer/sockets
 */

 #define SOCKETS_PROTOCOL_TCP 1

 native socket_server_start(ip[], port, protocol);
 native socket_server_set_maxclients(server_id, max_clients); // default: 500
 native socket_get_client_info(server_id, client_id, &bytes_sent, &bytes_received);
 native socket_send(server_id, client_id, message[]);

 forward OnSocketServerInitialized(server_id);
 forward OnSocketClientConnect(server_id, client_id, ip[], port);
 forward OnSocketClientDisconnect(server_id, client_id, reason);
 forward OnSocketServerReceive(server_id, client_id, message[], bytes);
 forward OnSocketServerError(server_id, error, errstr);