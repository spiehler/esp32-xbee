#ifndef ESP32_XBEE_WEBSOCKET_SERVER_H
#define ESP32_XBEE_WEBSOCKET_SERVER_H

#include <esp_http_server.h>
#include "keep_alive.h"

esp_err_t ws_handler(httpd_req_t *req);
esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd);
void wss_close_fd(httpd_handle_t hd, int sockfd);

void send_hello(void *arg);
void send_ping(void *arg);

bool client_not_alive_cb(wss_keep_alive_t h, int fd);
bool check_client_alive_cb(wss_keep_alive_t h, int fd);

void wss_server_send_messages(httpd_handle_t* server);

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const size_t max_clients = 10;

#endif //ESP32_XBEE_WEBSOCKET_SERVER_H
