#ifndef _FATALERROR_H_
#define _FATALERROR_H_

#include <err.h>

#define FATALERROR err(-1, "%s:%d:%s", __FILE__, __LINE__, __func__)

#endif /* _FATALERROR_H_ */
