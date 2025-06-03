#ifndef LUA_BINDINGS_H
#define LUA_BINDINGS_H

#include <lua.h>
#define MAX_COROUTINES 10

typedef struct {
    lua_State* co;
    int is_active;
    int64_t wake_up_time_us;
} LuaCoroutine;

typedef struct timer_task {
    lua_State* co;
    int64_t wakeup_time_ms;
    struct timer_task* next;
} timer_task_t;

extern timer_task_t* timer_list;

extern LuaCoroutine coroutines[MAX_COROUTINES];
extern int coroutine_count;

void register_lua_bindings(lua_State* L);
void timer_process(lua_State* L);

int l_settimeout(lua_State* L);
int l_delay(lua_State* L);

#endif // LUA_BINDINGS_H