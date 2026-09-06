#pragma once
#include <stdint.h>

typedef uint32_t KCtxBuf[6];

extern "C" int  ksetjmp(KCtxBuf buf);
extern "C" void klongjmp(KCtxBuf buf);
