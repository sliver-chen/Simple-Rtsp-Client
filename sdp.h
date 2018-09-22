#ifndef SDP_H
#define SDP_H

#include <time.h>

struct sdp_connection {
    char *nettype;
    char *addrtype;
    char *address;
};

struct sdp_bandwidth {
    char *bwtype;
    char *bandwidth;
};

struct sdp_payload {
    char *_payload;

    unsigned char proto_version;
    struct sdp_origin {
        char *username;
        long long int sess_id;
        long long int sess_version;
        char *nettype;
        char *addrtype;
        char *addr;
    } origin;
    char *session_name;
    char *information;
    char *uri;
    char **emails;
    size_t emails_count;
    char **phones;
    size_t phones_count;
    struct sdp_connection conn;
    struct sdp_bandwidth *bw;
    size_t bw_count;
    struct sdp_time {
        time_t start_time;
        time_t stop_time;
        struct sdp_repeat {
            time_t interval;
            time_t duration;
            time_t *offsets;
            size_t offsets_count;
        } *repeat;
        size_t repeat_count;
    } *times;
    size_t times_count;
    struct sdp_zone_adjustments {
        time_t adjust;
        time_t offset;
    } *zone_adjustments;
    size_t zone_adjustments_count;
    char *encrypt_key;
    char **attributes;
    size_t attributes_count;
    struct sdp_media {
        struct sdp_info {
            char *type;
            int port;
            int port_n;
            char *proto;
            int *fmt;
            size_t fmt_count;
        } info;
        char *title;
        struct sdp_connection conn;
        struct sdp_bandwidth *bw;
        size_t bw_count;
        char *encrypt_key;
        char **attributes;
        size_t attributes_count;
    } *medias;
    size_t medias_count;
};

struct sdp_payload *sdp_parse(const char *payload);
void sdp_destroy(struct sdp_payload *sdp);
void sdp_dump(struct sdp_payload *sdp);

char *sdp_get_attr(char **attr, size_t nattr, char *key);
int sdp_has_flag_attr(char **attr, size_t nattr, char *flag);

#endif
