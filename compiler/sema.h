#ifndef SEMA_H_
#define SEMA_H_

#include "parser.h"
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

void sema_run_global_analysis();

#ifdef __cplusplus
}
#endif

#endif // SEMA_H_
