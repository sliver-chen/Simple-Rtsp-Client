#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include "sdp.h"

static char *load_next_entry(char *p, char *key, char **value)
{
    char *endl;

    if (!p)
        goto fail;

    endl = strstr(p, "\r\n");
    if (!endl)
        endl = strchr(p, '\n');

    if (endl)
        while (*endl == '\r' || *endl == '\n')
            *endl++ = '\0';
    else
        endl = &p[strlen(p)];

    if (!p[0] || p[1] != '=')
        goto fail;

    *key = p[0];
    *value = &p[2];

    return endl;

fail:
    *key   = 0;
    *value = NULL;
    return NULL;
}

static char *split_values(char *p, char sep, char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    while (*p == sep)
        p++;
    while (*fmt) {
        char **s, *tmp;
        int *i;
        long long int *l;
        time_t *t;

        switch (*fmt++) {
        case 's':
            s = va_arg(va, char **);
            *s = p;
            tmp = strchr(p, sep);
            if (tmp) {
                p = tmp;
                while (*p == sep)
                    *p++ = '\0';
            } else {
                p = &p[strlen(p)];
            }
            break;
        case 'l':
            l = va_arg(va, long long int *);
            *l = strtoll(p, &tmp, 10);
            if (tmp == p)
                *p = 0;
            else
                p = tmp;
            break;
        case 'i':
            i = va_arg(va, int *);
            *i = strtol(p, &tmp, 10);
            if (tmp == p)
                *p = 0;
            else
                p = tmp;
            break;
        case 't':
            t = va_arg(va, time_t *);
            *t = strtol(p, &tmp, 10);
            if (tmp == p) {
                *p = 0;
            } else {
                p = tmp;
                switch (*p) {
                case 'd': *t *= 86400; p++; break;
                case 'h': *t *=  3600; p++; break;
                case 'm': *t *=    60; p++; break;
                }
            }
            break;
        }
        while (*p == sep)
            p++;
    }
    va_end(va);
    return p;
}

#define GET_CONN_INFO(connf_ptr) do {                              \
    if (key == 'c') {                                              \
        struct sdp_connection *c = connf_ptr;                      \
        split_values(value, ' ', "sss", &c->nettype, &c->addrtype, \
                     &c->address);                                 \
        p = load_next_entry(p, &key, &value);                      \
    }                                                              \
} while (0)

#define GET_BANDWIDTH_INFO(bw) do {                                \
    int n;                                                         \
    while (key == 'b') {                                           \
        ADD_ENTRY(bw);                                             \
        n = bw ## _count - 1;                                      \
        split_values(value, ':', "ss", &bw[n].bwtype,              \
                     &bw[n].bandwidth);                            \
        p = load_next_entry(p, &key, &value);                      \
    }                                                              \
} while (0)

#define LOAD_FACULTATIVE_STR(k, field) do {                        \
    if (key == k) {                                                \
        field = value;                                             \
        p = load_next_entry(p, &key, &value);                      \
    }                                                              \
} while (0)

#define LOAD_MULTIPLE_FACULTATIVE_STR(k, field) do {               \
    while (key == k) {                                             \
        ADD_ENTRY(field);                                          \
        field[field ## _count - 1] = value;                        \
        p = load_next_entry(p, &key, &value);                      \
    }                                                              \
} while (0)

#define ADD_ENTRY(field) do {                                      \
    field ## _count++;                                             \
    if (!field) {                                                  \
        field = calloc(1, sizeof(*field));                         \
    } else {                                                       \
        int n = field ## _count;                                   \
        field = realloc(field, sizeof(*field) * n);                \
        memset(&field[n - 1], 0, sizeof(*field));                  \
    }                                                              \
    if (!(field))                                                  \
        goto fail;                                                 \
} while (0)

struct sdp_payload *sdp_parse(const char *payload)
{
    struct sdp_payload *sdp = calloc(1, sizeof(*sdp));
    char *p, key, *value;

    if (!sdp)
        goto fail;

    p = sdp->_payload = strdup(payload);
    if (!p)
        goto fail;

    /* Protocol version (mandatory, only 0 supported) */
    p = load_next_entry(p, &key, &value);
    if (key != 'v')
        goto fail;
    sdp->proto_version = value[0] - '0';
    if (sdp->proto_version != 0 || value[1])
        goto fail;

    /* Origin field (mandatory) */
    p = load_next_entry(p, &key, &value);
    if (key != 'o')
        goto fail;
    else {
        struct sdp_origin *o = &sdp->origin;
        split_values(value, ' ', "sllsss", &o->username, &o->sess_id,
                     &o->sess_version, &o->nettype, &o->addrtype, &o->addr);
    }

    /* Session name field (mandatory) */
    p = load_next_entry(p, &key, &value);
    if (key != 's')
        goto fail;
    sdp->session_name = value;
    p = load_next_entry(p, &key, &value);

    /* Information field */
    LOAD_FACULTATIVE_STR('i', sdp->information);

    /* URI field */
    LOAD_FACULTATIVE_STR('u', sdp->uri);

    /* Email addresses */
    LOAD_MULTIPLE_FACULTATIVE_STR('e', sdp->emails);

    /* Phone numbers */
    LOAD_MULTIPLE_FACULTATIVE_STR('p', sdp->phones);

    /* Connection information */
    GET_CONN_INFO(&sdp->conn);

    /* Bandwidth fields */
    GET_BANDWIDTH_INFO(sdp->bw);

    /* Time fields (at least one mandatory) */
    do {
        struct sdp_time *tf;

        ADD_ENTRY(sdp->times);
        tf = &sdp->times[sdp->times_count - 1];
        split_values(value, ' ', "tt", &tf->start_time, &tf->stop_time);
        p = load_next_entry(p, &key, &value);

        while (key == 'r') {
            struct sdp_repeat *rf;

            ADD_ENTRY(tf->repeat);
            rf = &tf->repeat[tf->repeat_count - 1];
            value = split_values(value, ' ', "tt", &rf->interval, &rf->duration);
            while (*value) {
                int n = rf->offsets_count;
                ADD_ENTRY(rf->offsets);
                value = split_values(value, ' ', "t", &rf->offsets[n]);
            }
            p = load_next_entry(p, &key, &value);
        }
    } while (key == 't');

    /* Zone adjustments */
    if (key == 'z') {
        while (*value) {
            int n = sdp->zone_adjustments_count;
            struct sdp_zone_adjustments *za;

            ADD_ENTRY(sdp->zone_adjustments);
            za = &sdp->zone_adjustments[n];
            value = split_values(value, ' ', "tt", &za->adjust, &za->offset);
        }
        p = load_next_entry(p, &key, &value);
    }

    /* Encryption key */
    LOAD_FACULTATIVE_STR('k', sdp->encrypt_key);

    /* Media attributes */
    LOAD_MULTIPLE_FACULTATIVE_STR('a', sdp->attributes);

    /* Media descriptions */
    while (key == 'm') {
        struct sdp_media *md;

        ADD_ENTRY(sdp->medias);
        md = &sdp->medias[sdp->medias_count - 1];

        value = split_values(value, ' ', "s", &md->info.type);
        md->info.port = strtol(value, &value, 10);
        md->info.port_n = *value == '/' ? strtol(value + 1, &value, 10) : 0;
        value = split_values(value, ' ', "s", &md->info.proto);
        while (*value) {
            ADD_ENTRY(md->info.fmt);
            value = split_values(value, ' ', "i", &md->info.fmt[md->info.fmt_count - 1]);
        }
        p = load_next_entry(p, &key, &value);

        LOAD_FACULTATIVE_STR('i', md->title);
        GET_CONN_INFO(&md->conn);
        GET_BANDWIDTH_INFO(md->bw);
        LOAD_FACULTATIVE_STR('k', md->encrypt_key);
        LOAD_MULTIPLE_FACULTATIVE_STR('a', md->attributes);
    }

    return sdp;

fail:
    sdp_destroy(sdp);
    return NULL;
}

void sdp_destroy(struct sdp_payload *sdp)
{
    size_t i, j;

    if (sdp) {
        free(sdp->_payload);
        free(sdp->emails);
        free(sdp->phones);
        free(sdp->bw);
        for (i = 0; i < sdp->times_count; i++) {
            for (j = 0; j < sdp->times[i].repeat_count; j++)
                free(sdp->times[i].repeat[j].offsets);
            free(sdp->times[i].repeat);
        }
        free(sdp->times);
        free(sdp->zone_adjustments);
        free(sdp->attributes);
        for (i = 0; i < sdp->medias_count; i++) {
            free(sdp->medias[i].info.fmt);
            free(sdp->medias[i].bw);
            free(sdp->medias[i].attributes);
        }
        free(sdp->medias);
    }
    free(sdp);
}

char *sdp_get_attr(char **attr, size_t nattr, char *key)
{
    size_t i, klen = strlen(key);

    for (i = 0; i < nattr; i++)
        if (!strncmp(attr[i], key, klen) && attr[i][klen] == ':')
            return &attr[i][klen + 1];
    return NULL;
}

int sdp_has_flag_attr(char **attr, size_t nattr, char *flag)
{
    size_t i;

    for (i = 0; i < nattr; i++)
        if (!strcmp(attr[i], flag))
            return 1;
    return 0;
}

void sdp_dump(struct sdp_payload *sdp)
{
    size_t i, j, k;

    if (!sdp) {
        printf("invalid SDP\n");
        return;
    }

    printf("v=%d\n", sdp->proto_version);
    printf("o=%s %lld %lld %s %s %s\n", sdp->origin.username,
           sdp->origin.sess_id, sdp->origin.sess_version, sdp->origin.nettype,
           sdp->origin.addrtype, sdp->origin.addr);
    printf("s=%s\n", sdp->session_name);

    if (sdp->information) printf("i=%s\n", sdp->information);
    if (sdp->uri)         printf("u=%s\n", sdp->uri);

    for (i = 0; i < sdp->emails_count; i++) printf("e=%s\n", sdp->emails[i]);
    for (i = 0; i < sdp->phones_count; i++) printf("p=%s\n", sdp->phones[i]);

    if (sdp->conn.nettype && sdp->conn.addrtype && sdp->conn.address)
        printf("c=%s %s %s\n",
               sdp->conn.nettype, sdp->conn.addrtype, sdp->conn.address);

    for (i = 0; i < sdp->bw_count; i++)
        printf("b=%s:%s\n", sdp->bw[i].bwtype, sdp->bw[i].bandwidth);

    for (i = 0; i < sdp->times_count; i++) {
        struct sdp_time *t = &sdp->times[i];
        printf("t=%ld %ld\n", t->start_time, t->stop_time);
        for (j = 0; j < t->repeat_count; j++) {
            struct sdp_repeat *r = &t->repeat[j];
            printf("r=%ld %ld", r->interval, r->duration);
            for (k = 0; k < r->offsets_count; k++)
                printf(" %ld", r->offsets[k]);
            printf("\n");
        }
    }

    if (sdp->zone_adjustments_count) {
        printf("z=");
        for (i = 0; i < sdp->zone_adjustments_count; i++)
            printf("%ld %ld%s", sdp->zone_adjustments[i].adjust,
                   sdp->zone_adjustments[i].offset,
                   i + 1 < sdp->zone_adjustments_count ? " " : "");
        printf("\n");
    }

    if (sdp->encrypt_key)
        printf("k=%s\n", sdp->encrypt_key);

    for (i = 0; i < sdp->attributes_count; i++)
        printf("a=%s\n", sdp->attributes[i]);

    for (i = 0; i < sdp->medias_count; i++) {
        struct sdp_media *m   = &sdp->medias[i];
        struct sdp_info *info = &m->info;

        printf("m=%s %d", info->type, info->port);
        if (info->port_n)
            printf("/%d", info->port_n);
        printf(" %s", info->proto);
        for (j = 0; j < info->fmt_count; j++)
            printf(" %d", info->fmt[j]);
        printf("\n");

        if (m->title)        printf("i=%s\n", m->title);
        if (m->conn.nettype && m->conn.addrtype && m->conn.address)
            printf("c=%s %s %s\n",
                   m->conn.nettype, m->conn.addrtype, m->conn.address);
        for (j = 0; j < m->bw_count; j++)
            printf("b=%s:%s\n", m->bw[j].bwtype, m->bw[j].bandwidth);
        if (m->encrypt_key)  printf("k=%s\n", m->encrypt_key);
        for (j = 0; j < m->attributes_count; j++)
            printf("a=%s\n", m->attributes[j]);
    }
}
