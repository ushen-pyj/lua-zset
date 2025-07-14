#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#define ZSKIPLIST_MAXLEVEL 32
#define ZSKIPLIST_P 0.25 

typedef uint64_t objid;

typedef struct skiplistNode {
    objid ele;
    int64_t score;
    struct skiplistNode *backward;
    struct skiplistLevel {
        struct skiplistNode *forward;
        unsigned long span;
    } level[];
} skiplistNode;

typedef struct skiplist {
    struct skiplistNode *header, *tail;
    unsigned long length;
    int level;
} skiplist;

typedef struct zset {
    skiplist *zsl;
    int reverse;
} zset;

typedef struct zset_iterator {
    objid ele;
    int64_t score;
} zset_iterator;

zset *zsetCreate(int reverse);
skiplistNode *zslInsert(skiplist *zsl, int64_t score, objid ele);
skiplistNode *zslUpdateScore(skiplist *zsl, int64_t curscore, objid ele, int64_t newscore);
int zslDelete(skiplist *zsl, int64_t score, objid ele, skiplistNode **node);
void zslFree(zset *zset);
zset_iterator **zslGetRange(zset *zset, int start, int end);