#ifndef _SN_ENV_H_
#define _SN_ENV_H_

const char *skynet_getenv(const char *key);
void skynet_setenv(const char *key, const char *value);

class SNEnv
{
public:
	SNEnv();
	~SNEnv();
};

#endif
