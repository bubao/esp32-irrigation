#ifndef LUA_FUNCTIONS_H
#define LUA_FUNCTIONS_H

#include <lua.h>
#define LUA_FILE_PATH "/assets"

void init_littlefs();
void start_lua_task(lua_State *L);
void start_lua_system();
#endif // LUA_FUNCTIONS_H