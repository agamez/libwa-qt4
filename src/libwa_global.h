#ifndef LIBWA_GLOBAL_H
#define LIBWA_GLOBAL_H

#include <QtCore/QtGlobal>

#if !defined(LIBWA_API)
#  if defined(LIBWA_LIBRARY)
#      define LIBWA_API Q_DECL_EXPORT
#  else
#      define LIBWA_API Q_DECL_IMPORT
#  endif //LIBWA_LIBRARY
#endif //LIBWA_API

#endif // LIBWA_GLOBAL_H
