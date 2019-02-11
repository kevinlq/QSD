#ifndef QFRAMELESSHELPER_EXPORT_H
#define QFRAMELESSHELPER_EXPORT_H

#include "QFrameGlobal/GlobalInc.h"

#if defined(QFRAMELESSHELPER_LIBRARY)
#  define QFrameLessHelper_Export LQ_DECL_EXPORT
#else
#  define QFrameLessHelper_Export LQ_DECL_IMPORT
#endif

#endif //QFRAMELESSHELPER_EXPORT_H
