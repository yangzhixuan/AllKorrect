#pragma once
#include <cstdio>
#include <cstdlib>

extern const char* LogPrefix();

#define LOG(fmt,args...) printf("%s [Log] " fmt "\n",LogPrefix(),##args)
#define DBG(fmt,args...) printf(fmt "\n",##args)
#define ERR(fmt,args...) fprintf(stderr,"%s [Err] " fmt "\n",LogPrefix(),##args)
