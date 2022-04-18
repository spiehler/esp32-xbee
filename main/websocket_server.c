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

#include <esp_log.h>
#include <esp_event_base.h>
//#include "keep_alive.h"
#include "websocket_server.h"
#include "uart.h"

static const char *TAG = "WEBSOCKET_SERVER";
httpd_handle_t http_server = NULL;


esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len)
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }
    // If it was a PONG, update the keep-alive
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG)
    {
        ESP_LOGD(TAG, "Received PONG message");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                                               httpd_req_to_sockfd(req));

        // If it was a TEXT message, just echo it back
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
        ESP_LOGI(TAG, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "New client connected %d", sockfd);
    //wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    //return wss_keep_alive_add_client(h, sockfd);
    return 0;
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "Client disconnected %d", sockfd);
    //wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    //wss_keep_alive_remove_client(h, sockfd);
}

void send_hello(void *arg)
{
    static const char *data = "Hello client";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

void send_ping(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;
    ws_pkt.type = HTTPD_WS_TYPE_PING;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

void send_uart(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)resp_arg->data;
    ws_pkt.len = resp_arg->data_len;
    // text is not working due to wrong utf-8 sequence
    // maybe in future find a solution???
    //ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

bool client_not_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGE(TAG, "Client not alive, closing fd %d", fd);
    wss_keep_alive_remove_client(h, fd);
    httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
    return true;
}

bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGD(TAG, "Checking if client (fd=%d) is alive", fd);
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = wss_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK)
    {
        return true;
    }
    return false;
}

static void websocket_server_uart_handler(void* handler_args, esp_event_base_t base, int32_t length, void* buf) {
    if (http_server)
        { // httpd might not have been created by now
        size_t clients = max_clients;
        int client_fds[max_clients];
        if (httpd_get_client_list(http_server, &clients, client_fds) == ESP_OK)
        {
            for (size_t i = 0; i < clients; ++i)
            {
                int sock = client_fds[i];
                if (httpd_ws_get_fd_info(http_server, sock) == HTTPD_WS_CLIENT_WEBSOCKET)
                {
                    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
                    resp_arg->hd = http_server;
                    resp_arg->fd = sock;
                    resp_arg->data = buf;
                    resp_arg->data_len = length;
                    if (httpd_queue_work(resp_arg->hd, send_uart, resp_arg) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "httpd_queue_work failed!");
                        break;
                    }
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "httpd_get_client_list failed!");
            return;
        }
    }
}

// Get all clients and send async message
void wss_server_send_messages(httpd_handle_t *server)
{
    http_server = *server;
    uart_register_read_handler(websocket_server_uart_handler);
}
