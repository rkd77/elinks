#ifndef EL__PROTOCOL_MAP_H
#define EL__PROTOCOL_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

void save_in_uri_map(char *url, char *pos);
char *get_url_pos(char *url);

#ifdef __cplusplus
}
#endif

#endif /* EL__PROTOCOL_MAP_H */
