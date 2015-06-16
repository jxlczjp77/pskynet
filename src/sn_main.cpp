#include "skynet.h"
#include "sn_env.h"
#include "sn_log.h"
#include "sn_impl.h"
#include "sn_common.h"
#include "sn_server.h"

#include "lua.hpp"

#ifdef _DEBUG
#if defined(WIN32) || defined(WIN64)
#include "vld.h"
#endif
#endif

static int optint(const char *key, int opt)
{
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		char tmp[20];
		sn_sprintf(tmp, sizeof(tmp), "%d", opt);
		skynet_setenv(key, tmp);
		return opt;
	}
	return strtol(str, NULL, 10);
}

static const char *optstring(const char *key, const char * opt)
{
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		if (opt) {
			skynet_setenv(key, opt);
			opt = skynet_getenv(key);
		}
		return opt;
	}
	return str;
}

static string exec_path()
{
	char tmp[1024];
	size_t nCount = 0;
	if (uv_exepath(tmp, &nCount) != 0) {
		return string();
	}
	tmp[nCount] = '\0';
	return tmp;
}

// 默认将当前可执行文件路径下的一些基础路径加上，省得每次都要在config文件设置
static const char *filter_env(const char *key, const char *val)
{
	static string ret;
	ret.clear();

	do {
		string exePath = exec_path();
		if (exePath.empty()) {
			break;
		}

		if (strcmp(key, "cpath") == 0) {

		}

		return ret.c_str();
	} while (false);
	return val;
}

static bool _init_env(lua_State *L)
{
	lua_pushnil(L);  /* first key */
	while (lua_next(L, -2) != 0) {
		int keyt = lua_type(L, -2);
		if (keyt != LUA_TSTRING) {
			LogError("Invalid config table\n");
			return false;
		}

		const char * key = lua_tostring(L, -2);
		if (lua_type(L, -1) == LUA_TBOOLEAN) {
			int b = lua_toboolean(L, -1);
			skynet_setenv(key, b ? "true" : "false");
		}
		else {
			const char * value = lua_tostring(L, -1);
			if (value == NULL) {
				LogError("Invalid config table key = %s\n", key);
				return false;
			}
			skynet_setenv(key, value);
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return true;
}

static const char * load_config = "\
	local config_name = ...\n\
	local f = assert(io.open(config_name))\n\
	local code = assert(f:read \'*a\')\n\
	local function getenv(name) return assert(os.getenv(name), name) end\n\
	code = string.gsub(code, \'%$([%w_%d]+)\', getenv)\n\
	f:close()\n\
	local result = {}\n\
	assert(load(code,\'=(load)\',\'t\',result))()\n\
	return result\n\
";

int skynet_main(int argc, char *argv[])
{
	const char * config_file = NULL;
	if (argc > 1) {
		config_file = argv[1];
	}
	else {
		LogError("Need a config file. Please read skynet wiki : https://github.com/cloudwu/skynet/wiki/Config\n"
			"usage: skynet configfilename\n");
		return 1;
	}

	LogRaw("\n\n=========== Skynet Start ===========\n\n");
	SNEnv env;

	skynet_config config;

	lua_State *L = lua_newstate(skynet_lalloc, NULL);
	luaL_openlibs(L);	// link lua lib

	int err = luaL_loadstring(L, load_config);
	assert(err == LUA_OK);
	lua_pushstring(L, config_file);

	err = lua_pcall(L, 1, 1, 0);
	if (err) {
		LogError("%s\n", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}

	if (!_init_env(L)) {
		return 1;
	}

	config.thread = optint("thread", 8);
	config.module_path = optstring("cpath", "./cservice/?.so");
	config.harbor = optint("harbor", 1);
	config.bootstrap = optstring("bootstrap", "snlua bootstrap");
	config.logger = optstring("logger", NULL);

	lua_close(L);

	SNServer snserver(config.harbor, config.module_path);
	skynet_start(snserver, &config);

	LogRaw("\n\n=========== Skynet Exit ===========\n\n");
	return 0;
}
