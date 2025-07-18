#include "zset.h"
#include "lua.h"
#include "lauxlib.h"

static const char *zset_metatable = "lua-zset";

typedef struct lua_zset {
    zset *ob;
    int score_table_ref;
} lua_zset;

int
lfree(lua_State *L) {
    lua_zset *zset = luaL_checkudata(L, 1, zset_metatable);
    if (zset->ob != NULL) {
        zslFree(zset->ob);
        zset->ob = NULL;
    }
    if (zset->score_table_ref != LUA_NOREF) {
        int ref = zset->score_table_ref;
        zset->score_table_ref = LUA_NOREF;
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    return 0;
}

int
lcreate(lua_State *L) {
    int max_length = luaL_optinteger(L, 1, 0);
    int reverse = luaL_optinteger(L, 2, 0);
    lua_zset *zset = lua_newuserdata(L, sizeof(lua_zset));
    zset->ob = zsetCreate(max_length, reverse);
    lua_newtable(L);
    zset->score_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_getmetatable(L, zset_metatable);
    lua_setmetatable(L, -2);
    return 1;
}

int lupdate(lua_State *L) {
    lua_zset *zset = luaL_checkudata(L, 1, zset_metatable);
    objid ele = (objid)luaL_checknumber(L, 2);
    double newscore = luaL_checknumber(L, 3);
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_gettable(L, -2);
    
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        // printf("insert newscore: %ld %f\n", ele, newscore);
        skiplistNode *node = zslInsert(zset->ob->zsl, newscore, ele);
        if (node == NULL) {
            lua_pushnil(L);
            return 1;
        }
    } else {
        double curscore = lua_tonumber(L, -1);
        // printf("update curscore: %ld %f\n", ele, curscore);
        lua_pop(L, 2);
        skiplistNode *node = zslUpdateScore(zset->ob->zsl, curscore, ele, newscore);
        if (node == NULL) {
            lua_pushnil(L);
            return 1;
        }
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_pushnumber(L, newscore);
    lua_settable(L, -3);
    lua_pop(L, 1);
    
    lua_newtable(L);
    int len = zslGetLength(zset->ob);
    if (zset->ob->max_length > 0 && len > zset->ob->max_length) {
        objid *remove_ele = zslDeleteRangeByRank(zset->ob->zsl, zset->ob->max_length + 1, len);
        for (int i = 0; i < len - zset->ob->max_length; i++) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
            lua_pushnumber(L, remove_ele[i]);
            lua_pushnil(L);
            lua_settable(L, -3);
            lua_pop(L, 1);
            
            lua_pushinteger(L, i + 1);     
            lua_pushnumber(L, remove_ele[i]); 
            lua_settable(L, -3);    
        }
        free(remove_ele);
    }
 
    return 1;
}

int lgetscore(lua_State *L) {
    lua_zset *zset = luaL_checkudata(L, 1, zset_metatable);
    objid ele = (objid)luaL_checknumber(L, 2);
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_gettable(L, -2);
    
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        lua_pushnil(L);
    } else {
        lua_remove(L, -2);
    }
    
    return 1;
}

int 
ldelete(lua_State *L) {
    lua_zset *zset = luaL_checkudata(L, 1, zset_metatable);
    objid ele = (objid)luaL_checknumber(L, 2);
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }
    double score = lua_tonumber(L, -1);
    lua_pop(L, 2);

    zslDelete(zset->ob->zsl, score, ele, NULL);
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_pushnil(L);
    lua_settable(L, -3);
    lua_pop(L, 1);
    
    return 0;
}

int lrange(lua_State *L) {
    lua_zset *zset = luaL_checkudata(L, 1, zset_metatable);
    int start = luaL_checknumber(L, 2);
    int end = luaL_checknumber(L, 3);
    
    zset_iterator **iter = zslGetRange(zset->ob, start, end);
    lua_newtable(L);
    for (int i = 0; iter[i] != NULL; i++) {
        lua_newtable(L);
        lua_pushstring(L, "key");
        lua_pushnumber(L, iter[i]->ele);
        lua_settable(L, -3);
        lua_pushstring(L, "score");
        lua_pushnumber(L, iter[i]->score);
        lua_settable(L, -3);
        lua_pushstring(L, "rank");
        lua_pushnumber(L, start + i + 1);
        lua_settable(L, -3);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static const struct luaL_Reg zsetlib[] = {
    {"create", lcreate},
    {NULL, NULL}
};

static const struct luaL_Reg zset_methods[] = {
    {"update", lupdate},
    {"getscore", lgetscore},
    {"delete", ldelete},
    {"range", lrange},
    {NULL, NULL}
};

int luaopen_zset(lua_State *L) {
    luaL_newmetatable(L, zset_metatable);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, zset_methods, 0);
    lua_pushcfunction(L, lfree);
    lua_setfield(L, -2, "__gc");
    lua_pushstring(L, "protected");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);

    luaL_newlib(L, zsetlib);
    return 1;
}

