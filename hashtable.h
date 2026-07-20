#include <stddef.h>

struct HNode {
  HNode* next;
  int hcode;
};

struct HTab {
  HNode** tab = NULL;
  size_t mask;
  size_t size;
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos;
};

HNode* hm_lookup(HMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
void hm_insert(HMap* hmap, HNode* node);
HNode* hm_delete(HMap* hmap, HNode* key, bool (*eq)(HNode*, HNode*));
void hm_clear(HMap* hmap);
size_t hm_size(HMap* hmap);

