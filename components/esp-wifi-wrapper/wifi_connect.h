#ifndef _WIFI_CONNECT_H_
#define _WIFI_CONNECT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_CONNECT_MODE_NONE = 0,
    WIFI_CONNECT_MODE_AP,
    WIFI_CONNECT_MODE_STA
} wifi_connect_mode_t;
// typedef enum {
//     WIFI_MODE_NONE = 0,
//     WIFI_MODE_AP,
//     WIFI_MODE_STA
// } wifi_mode_t;

wifi_connect_mode_t wifi_connect(void);

#ifdef __cplusplus
}
#endif

#endif // _WIFI_CONNECT_H_
