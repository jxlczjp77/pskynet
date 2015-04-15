#ifndef _SNMODULE_H_
#define _SNMODULE_H_
#include "sn_common.h"
#include "uv.h"

typedef void * (*sn_dl_create)(void);
typedef int(*sn_dl_init)(void *pInst, SNContext *pContext, const char *pParam);
typedef void(*sn_dl_release)(void *pInst);

#if defined(WIN32) || defined(WIN64)
#define LIB_HANDLER HMODULE
#else
#define LIB_HANDLER void *
#endif

class SNModule
{
public:
	SNModule();
	~SNModule();

	bool Load(const string &path, const string &name);
	void Free();

	void *Create();
	int Init(void *pInst, SNContext *pContext, const char *pParam);
	void Release(void *pInst);

private:
	string m_path;
	LIB_HANDLER m_handler;

	sn_dl_create m_pCreateFun;
	sn_dl_init m_pInitFun;
	sn_dl_release m_pReleaseFun;
};
typedef std::shared_ptr<SNModule> SNModulePtr;

class SNModuleMgr
{
public:
	SNModuleMgr();
	~SNModuleMgr();

	void Free();
	
	void SetPaths(const string &paths);
	SNModule *Load(const string &moduleName);

private:
	int count;
	string m_config;
	list<string> m_paths;
	map<string, SNModulePtr> m_modules;
};

#endif
