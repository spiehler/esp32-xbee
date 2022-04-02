/*
 * This file is part of the ESP32-XBee distribution (https://github.com/nebkat/esp32-xbee).
 * Copyright (c) 2019 Nebojsa Cvetkovic.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */



// handler to send data to websocket client
static void websocket_server_uart_handler(void* handler_args, esp_event_base_t base, int32_t length, void* buf) {
    socket_client_t *client, *client_tmp;
    SLIST_FOREACH_SAFE(client, &socket_client_list, next, client_tmp) {
        int sent = write(client->socket, buf, length);
        if (sent < 0) {
            ESP_LOGE(TAG, "Could not write to %s socket: %d %s", SOCKTYPE_NAME(client->type), errno, strerror(errno));
            socket_client_remove(client);
        } else {
            stream_stats_increment(stream_stats, 0, sent);
        }
    }
}

static void socket_server_task(void *ctx) {
    uart_register_read_handler(websocket_server_uart_handler);

    config_color_t status_led_color = config_get_color(CONF_ITEM(KEY_CONFIG_WEBSOCKET_SERVER_COLOR));
    if (status_led_color.rgba != 0) status_led = status_led_add(status_led_color.rgba, STATUS_LED_STATIC, 500, 2000, 0);

    stream_stats = stream_stats_new("websocket_server");

    while (true) {
        SLIST_INIT(&socket_client_list);

        socket_tcp_init();
        socket_udp_init();

        // Accept/receive loop
        buffer = malloc(BUFFER_SIZE);
        fd_set socket_set;
        while (true) {
            // Reset all selected
            FD_ZERO(&socket_set);

            // New TCP/UDP connections
            FD_SET(sock_tcp, &socket_set);
            FD_SET(sock_udp, &socket_set);

            int maxfd = MAX(sock_tcp, sock_udp);

            // Existing connections
            socket_client_t *client;
            SLIST_FOREACH(client, &socket_client_list, next) {
                FD_SET(client->socket, &socket_set);
                maxfd = MAX(maxfd, client->socket);
            }

            // Wait for activity on one of selected
            int err = select(maxfd + 1, &socket_set, NULL, NULL, NULL);
            ERROR_ACTION(TAG, err < 0, goto _error, "Could not select socket to receive from: %d %s", errno, strerror(errno))

            // Accept new connections
            if (FD_ISSET(sock_tcp, &socket_set)) socket_tcp_accept();
            if (FD_ISSET(sock_udp, &socket_set)) socket_udp_accept();

            // Receive from existing connections
            socket_clients_receive(&socket_set);
        }

        _error:
        destroy_socket(&sock_tcp);
        destroy_socket(&sock_udp);
        socket_client_t *client, *client_tmp;
        SLIST_FOREACH_SAFE(client, &socket_client_list, next, client_tmp) {
            destroy_socket(&client->socket);
            SLIST_REMOVE(&socket_client_list, client, socket_client_t, next);
            free(client);
        }

        free(buffer);
    }
}

void websocket_server_init() {
    if (!config_get_bool1(CONF_ITEM(KEY_CONFIG_WEBSOCKET_SERVER_ACTIVE))) return;

    xTaskCreate(socket_server_task, "websocket_server_task", 4096, NULL, TASK_PRIORITY_INTERFACE, NULL);
}