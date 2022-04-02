#ifndef ESP32_XBEE_WEB_SERVER_H
#define ESP32_XBEE_WEB_SERVER_H

/*
note: change config to allow more connections
*/
#define HTTPD_CONFIG() {                                \
        .task_priority      = tskIDLE_PRIORITY+5,       \
        .stack_size         = 4096,                     \
        .core_id            = tskNO_AFFINITY,           \
        .server_port        = 80,                       \
        .ctrl_port          = 32768,                    \
        .max_open_sockets   = 10,                       \
        .max_uri_handlers   = 10,                       \
        .max_resp_headers   = 10,                       \
        .backlog_conn       = 5,                        \
        .lru_purge_enable   = false,                    \
        .recv_wait_timeout  = 5,                        \
        .send_wait_timeout  = 5,                        \
        .global_user_ctx = NULL,                        \
        .global_user_ctx_free_fn = NULL,                \
        .global_transport_ctx = NULL,                   \
        .global_transport_ctx_free_fn = NULL,           \
        .open_fn = NULL,                                \
        .close_fn = NULL,                               \
        .uri_match_fn = NULL                            \
}

void web_server_init();

#endif //ESP32_XBEE_WEB_SERVER_H
