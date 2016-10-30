/*!
Global definitions
*/
#ifndef _CELLC_COMMON_HG_5344656345_H
#define _CELLC_COMMON_HG_5344656345_H

//Control Cells-Lib compilation and linkage with 3 makros in your build environment:
// BUILDING_CELLS_DLL - build as DLL, exporting API
// USING_CELLS_DLL - used by your application as DLL
// EMBEDDING_CELLS -CoreLib is built-in in your application. Use it for embedding minor parts of core lib e.g. some helper classes.

#ifdef BUILDING_CELLS_DLL
# define CELLS_EXPORT Q_DECL_EXPORT
#else
# define CELLS_EXPORT Q_DECL_IMPORT
#endif

#ifdef EMBEDDING_CELLS
# undef CELLS_EXPORT
# define CELLS_EXPORT
#endif

#endif // HG
