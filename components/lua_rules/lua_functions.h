#ifndef LUA_FUNCTIONS_H
#define LUA_FUNCTIONS_H

#include <lua.h>
#define LUA_FILE_PATH "/assets"

void init_spiffs();
lua_State* init_lua();
void register_lua_script(lua_State *L, const char *filename);
void start_lua_task(lua_State *L);
void register_all_lua_scripts(lua_State* L, const char* directory);
#endif // LUA_FUNCTIONS_H