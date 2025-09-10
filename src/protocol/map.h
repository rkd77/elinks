#ifndef EL__PROTOCOL_MAP_H
#define EL__PROTOCOL_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

void save_in_uri_map(char *url, char *pos);
const char *get_url_pos(char *url);
void del_position(char *url);


#ifdef __cplusplus
}
#endif

#endif /* EL__PROTOCOL_MAP_H */
