#include "zset.h"

void 
_zslDeleteNode(skiplist *zsl, skiplistNode *x, skiplistNode **update) {
    int i;
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        zsl->tail = x->backward;
    }
    while(zsl->level > 1 && zsl->header->level[zsl->level-1].forward == NULL)
        zsl->level--;
    zsl->length--;
}

skiplistNode *
_zslCreateNode(int level, int64_t score, objid ele) {
    skiplistNode *zn =
        malloc(sizeof(*zn)+level*sizeof(struct skiplistLevel));
    zn->score = score;
    zn->ele = ele;
    return zn;
}

skiplist *
_zslCreate(void) {
    int j;
    skiplist *zsl;

    zsl = malloc(sizeof(*zsl));
    zsl->level = 1;
    zsl->length = 0;
    zsl->header = _zslCreateNode(ZSKIPLIST_MAXLEVEL,0,0);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    zsl->header->backward = NULL;
    zsl->tail = NULL;
    return zsl;
}

void 
_zslFreeNode(skiplistNode *node) {
    free(node);
}

void 
_zslFree(skiplist *zsl) {
    skiplistNode *node = zsl->header->level[0].forward, *next;
    free(zsl->header);
    while(node) {
        next = node->level[0].forward;
        _zslFreeNode(node);
        node = next;
    }
    free(zsl);
}

int
_objid_cmp(objid a, objid b) {
    return a - b;
}

int _zslRandomLevel(void) {
    static const int threshold = ZSKIPLIST_P*RAND_MAX;
    int level = 1;
    while (rand() < threshold)
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

zset *
zsetCreate(int max_length, int reverse) {
    zset *zset = malloc(sizeof(*zset));
    zset->zsl = _zslCreate();
    zset->reverse = reverse;
    zset->max_length = max_length;
    return zset;
}

void
zslFree(zset *zset) {
    _zslFree(zset->zsl);
    free(zset);
}

skiplistNode *
zslUpdateScore(skiplist *zsl, int64_t curscore, objid ele, int64_t newscore) {
    skiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    /* We need to seek to element to update to start: this is useful anyway,
     * we'll have to update or remove it. */
    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
                (x->level[i].forward->score < curscore ||
                    (x->level[i].forward->score == curscore &&
                     _objid_cmp(x->level[i].forward->ele,ele) < 0)))
        {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    /* Jump to our element: note that this function assumes that the
     * element with the matching score exists. */
    x = x->level[0].forward;
    assert(x && curscore == x->score && _objid_cmp(x->ele,ele) == 0);

    /* If the node, after the score update, would be still exactly
     * at the same position, we can just update the score without
     * actually removing and re-inserting the element in the skiplist. */
    if ((x->backward == NULL || x->backward->score < newscore) &&
        (x->level[0].forward == NULL || x->level[0].forward->score > newscore))
    {
        x->score = newscore;
        return x;
    }

    /* No way to reuse the old node: we need to remove and insert a new
     * one at a different place. */
    _zslDeleteNode(zsl, x, update);
    skiplistNode *newnode = zslInsert(zsl,newscore,x->ele);
    /* We reused the old node x->ele SDS string, free the node now
     * since zslInsert created a new one. */
    x->ele = 0;
    _zslFreeNode(x);
    return newnode;
}

skiplistNode *
zslInsert(skiplist *zsl, int64_t score, objid ele) {
    skiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long rank[ZSKIPLIST_MAXLEVEL];
    int i, level;

    assert(score != NAN);
    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        /* store rank that is crossed to reach the insert position */
        rank[i] = i == (zsl->level-1) ? 0 : rank[i+1];
        while (x->level[i].forward &&
                (x->level[i].forward->score < score ||
                    (x->level[i].forward->score == score &&
                    _objid_cmp(x->level[i].forward->ele,ele) < 0)))
        {
            rank[i] += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }
    /* we assume the element is not already inside, since we allow duplicated
     * scores, reinserting the same element should never happen since the
     * caller of zslInsert() should test in the hash table if the element is
     * already inside or not. */
    level = _zslRandomLevel();
    if (level > zsl->level) {
        for (i = zsl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = zsl->header;
            update[i]->level[i].span = zsl->length;
        }
        zsl->level = level;
    }
    x = _zslCreateNode(level,score,ele);
    for (i = 0; i < level; i++) {
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;

        /* update span covered by update[i] as x is inserted here */
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    /* increment span for untouched levels */
    for (i = level; i < zsl->level; i++) {
        update[i]->level[i].span++;
    }

    x->backward = (update[0] == zsl->header) ? NULL : update[0];
    if (x->level[0].forward)
        x->level[0].forward->backward = x;
    else
        zsl->tail = x;
    zsl->length++;
    return x;
}

int 
zslDelete(skiplist *zsl, int64_t score, objid ele, skiplistNode **node) {
    skiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
                (x->level[i].forward->score < score ||
                    (x->level[i].forward->score == score &&
                     _objid_cmp(x->level[i].forward->ele,ele) < 0)))
        {
            x = x->level[i].forward;
        }
        update[i] = x;
    }
    /* We may have multiple elements with the same score, what we need
     * is to find the element with both the right score and object. */
    x = x->level[0].forward;
    if (x && score == x->score && _objid_cmp(x->ele,ele) == 0) {
        _zslDeleteNode(zsl, x, update);
        if (!node)
            _zslFreeNode(x);
        else
            *node = x;
        return 1;
    }
    return 0; /* not found */
}

skiplistNode* zslGetElementByRank(skiplist *zsl, unsigned long rank) {
    skiplistNode *x;
    unsigned long traversed = 0;
    int i;

    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) <= rank)
        {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        if (traversed == rank) {
            return x;
        }
    }
    return NULL;
}

objid *zslDeleteRangeByRank(skiplist *zsl, unsigned int start, unsigned int end) {
    skiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long traversed = 0, removed = 0;
    int i;

    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) < start) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    traversed++;
    x = x->level[0].forward;
    objid *remove_ele = malloc(sizeof(objid) * (end - start + 1));
    while (x && traversed <= end) {
        skiplistNode *next = x->level[0].forward;
        remove_ele[removed] = x->ele;
        _zslDeleteNode(zsl, x, update);
        _zslFreeNode(x);
        removed++;
        traversed++;
        x = next;
    }
    return remove_ele;
}

zset_iterator **
zslGetRange(zset *zset, int start, int end) {
    if (start < 0) start = 0;
    if (end < start) {
        zset_iterator **iter = malloc(sizeof(*iter));
        iter[0] = NULL;
        return iter;
    }
    
    int rangelen = end - start + 1;
    if (rangelen <= 0) {
        zset_iterator **iter = malloc(sizeof(*iter));
        iter[0] = NULL;
        return iter;
    }

    zset_iterator **iter = malloc(sizeof(*iter) * (rangelen + 1));
    for (int i = 0; i < rangelen; i++) {
        iter[i] = malloc(sizeof(*iter[i]));
    }

    skiplistNode *ln;
    skiplist *zsl = zset->zsl;
    
    if (!zset || !zsl || !zsl->header) {
        for (int i = 0; i < rangelen; i++) {
            free(iter[i]);
        }
        free(iter);
        zset_iterator **empty_iter = malloc(sizeof(*empty_iter));
        empty_iter[0] = NULL;
        return empty_iter;
    }

    if (zset->reverse) {
        ln = zsl->tail;
        if (start > 0 && zsl->length > 0) {
            unsigned long rank = zsl->length - start;
            if (rank > 0) {
                ln = zslGetElementByRank(zsl, rank);
            } else {
                ln = NULL;
            }
        }
    } else {
        ln = zsl->header->level[0].forward;
        if (start > 0 && zsl->length > 0) {
            unsigned long rank = start + 1;
            if (rank <= zsl->length) {
                ln = zslGetElementByRank(zsl, rank);
            } else {
                ln = NULL;
            }
        }
    }
    
    int i = 0;
    while (rangelen > 0 && ln != NULL) {
        iter[i]->ele = ln->ele;
        iter[i]->score = ln->score;
        i++;
        rangelen--;
        ln = zset->reverse ? ln->backward : ln->level[0].forward;
    }
    
    iter[i] = NULL;
    return iter;
}

int zslGetLength(zset *zset) {
    return zset->zsl->length;
}

