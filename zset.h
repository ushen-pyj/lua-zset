#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#define ZSKIPLIST_MAXLEVEL 32
#define ZSKIPLIST_P 0.25 

typedef uint64_t objid;

typedef struct skiplistNode {
    objid ele;
    double score;
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
} zset;

zset *zsetCreate();
skiplistNode *zslInsert(skiplist *zsl, double score, objid ele);
skiplistNode *zslUpdateScore(skiplist *zsl, double curscore, objid ele, double newscore);
int zslDelete(skiplist *zsl, double score, objid ele, skiplistNode **node);
void zslFree(zset *zset);