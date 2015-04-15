#include "skynet.h"
#include "sn_env.h"
#include "sn_malloc.h"
#include "sn_common.h"

#include "lua.hpp"

#include <stdlib.h>
#include <assert.h>

struct skynet_env
{
	SpinLock m_mutex;
	lua_State *L;
};

static struct skynet_env *E = NULL;

const char *skynet_getenv(const char *key)
{
	SpinLock::scoped_lock lock(E->m_mutex);

	lua_State *L = E->L;
	lua_getglobal(L, key);
	const char * result = lua_tostring(L, -1);
	lua_pop(L, 1);

	return result;
}

void skynet_setenv(const char *key, const char *value)
{
	SpinLock::scoped_lock lock(E->m_mutex);

	lua_State *L = E->L;
	lua_getglobal(L, key);
	assert(lua_isnil(L, -1));
	lua_pop(L, 1);
	lua_pushstring(L, value);
	lua_setglobal(L, key);
}

SNEnv::SNEnv()
{
	E = (skynet_env *)sn_malloc(sizeof(*E));
	E->m_mutex.v_ = 0;
	E->L = luaL_newstate();
}

SNEnv::~SNEnv()
{
	assert(E != NULL);
	lua_close(E->L);
	sn_free(E);
}
