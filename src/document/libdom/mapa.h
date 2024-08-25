#ifndef EL__DOCUMENT_LIBDOM_MAPA_H
#define EL__DOCUMENT_LIBDOM_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

struct el_node_elem {
	int offset;
	void *node;
};

struct el_mapa {
	unsigned int size;
	unsigned int allocated;
	struct el_node_elem *table;
};

void save_in_map(void *m, void *node, int length);
void save_in_map2(void *m, void *node, int length);

void save_offset_in_map(void *m, void *node, int offset);
void *create_new_element_map(void);
void *create_new_element_map_rev(void);
void delete_map(void *m);
void delete_map_rev(void *m);

void *find_in_map(void *m, int offset);

void *find_in_map2(void *m, int offset);

int find_offset(void *m, void *node);

void sort_nodes(void *m);

#ifdef __cplusplus
}
#endif

#endif
