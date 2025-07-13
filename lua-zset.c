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
    // printf("free zset: %p\n", zset);
    zslFree(zset->ob);
    luaL_unref(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    return 0;
}

int
lcreate(lua_State *L) {
    lua_zset *zset = lua_newuserdata(L, sizeof(lua_zset));
    zset->ob = zsetCreate();
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
    
    return 0;
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
    
    // 从跳表中删除
    zslDelete(zset->ob->zsl, 0.0, ele, NULL);
    
    // 从分数表中删除
    lua_rawgeti(L, LUA_REGISTRYINDEX, zset->score_table_ref);
    lua_pushnumber(L, ele);
    lua_pushnil(L);
    lua_settable(L, -3);
    lua_pop(L, 1);
    
    return 0;
}

static const struct luaL_Reg zsetlib[] = {
    {"create", lcreate},
    {NULL, NULL}
};

static const struct luaL_Reg zset_methods[] = {
    {"update", lupdate},
    {"getscore", lgetscore},
    {"delete", ldelete},
    {NULL, NULL}
};

int luaopen_zset(lua_State *L) {
    luaL_newmetatable(L, zset_metatable);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, zset_methods, 0);
    lua_pushcfunction(L, lfree);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lfree);
    lua_setfield(L, -2, "__close");
    lua_pushstring(L, "protected");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);

    luaL_newlib(L, zsetlib);
    return 1;
}

