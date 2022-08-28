#include "llhttp.h"
#include <lua.h>

int lua_llhttp_hello(lua_State *L) {
  lua_pushliteral(L, "Hello, llhttp!");
  return 1;
}

int luaopen_llhttp_core(lua_State *L) {
  lua_newtable(L);
  lua_pushcfunction(L, lua_llhttp_hello);
  lua_setfield(L, -2, "hello");
  return 1;
}
