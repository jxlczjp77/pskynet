#include <boost/algorithm/string.hpp>
#include "sn_module.h"
#include "sn_log.h"

SNModule::SNModule()
{
	m_pCreateFun = NULL;
	m_pInitFun = NULL;
	m_pReleaseFun = NULL;
	m_handler = 0;
}

SNModule::~SNModule()
{
	assert(m_handler == NULL);
}

void SNModule::Free()
{
	if (m_handler) {
		uv_lib_t lib_;
		lib_.handle = m_handler;
		uv_dlclose(&lib_);
		m_handler = NULL;
	}
}

bool SNModule::Load(const string &path, const string &name)
{
	uv_lib_t lib_;
	if (uv_dlopen(path.c_str(), &lib_) == -1) {
		return false;
	}
	assert(lib_.handle != 0);

	string createName = name + "_create";
	string initName = name + "_init";
	string releaseName = name + "_release";
	sn_dl_create pCreateFun = NULL;
	sn_dl_init pInitFun = NULL;
	sn_dl_release pReleaseFun = NULL;

	uv_dlsym(&lib_, createName.c_str(), (void **)&pCreateFun);
	uv_dlsym(&lib_, initName.c_str(), (void **)&pInitFun);
	uv_dlsym(&lib_, releaseName.c_str(), (void **)&pReleaseFun);

	if (pInitFun == NULL) {
		uv_dlclose(&lib_);
		return false;
	}

	m_handler = lib_.handle;
	m_pCreateFun = pCreateFun;
	m_pInitFun = pInitFun;
	m_pReleaseFun = pReleaseFun;
	m_path = path;
	return true;
}

void *SNModule::Create()
{
	if (m_pCreateFun) {
		return m_pCreateFun();
	}
	else {
		return NULL;
	}
}

int SNModule::Init(void *pInst, SNContext *pContext, const char *pParam)
{
	return m_pInitFun(pInst, pContext, pParam);
}

void SNModule::Release(void *pInst)
{
	if (m_pReleaseFun) {
		m_pReleaseFun(pInst);
	}
}

//////////////////////////////////////////////////////////////////////////
// 
SNModuleMgr::SNModuleMgr()
{

}

SNModuleMgr::~SNModuleMgr()
{
}

void SNModuleMgr::Free()
{
	for (auto it = m_modules.begin(); it != m_modules.end(); ++it) {
		it->second->Free();
	}
	m_modules.clear();
}

void SNModuleMgr::SetPaths(const string &paths)
{
	m_config = paths;
	boost::split(m_paths, paths, boost::is_any_of(";"));
	for (auto it = m_paths.begin(); it != m_paths.end();) {
		if (it->find('?') == string::npos) {
			LogWarn("Invalid C service path : %s", it->c_str());
			it = m_paths.erase(it);
			continue;
		}
		++it;
	}
}

SNModule *SNModuleMgr::Load(const string &moduleName)
{
	auto it = m_modules.find(moduleName);
	if (it != m_modules.end()) {
		return it->second.get();
	}

	SNModulePtr pModule(new SNModule);
	for (auto it = m_paths.begin(); it != m_paths.end(); ++it) {
		string path = *it;
		boost::replace_first(path, "?", moduleName);
		if (pModule->Load(path, moduleName)) {
			m_modules[moduleName] = pModule;
			return pModule.get();
		}
	}
	return NULL;
}
