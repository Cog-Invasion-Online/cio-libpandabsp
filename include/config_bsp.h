/**
 * PANDA3D BSP LIBRARY
 * 
 * Copyright (c) Brian Lach <brianlach72@gmail.com>
 * All rights reserved.
 *
 * @file config_bsp.h
 * @author Brian Lach
 * @date March 27, 2018
 */

#ifndef CONFIG_BSP_H
#define CONFIG_BSP_H

#include <dconfig.h>

#ifdef BUILDING_LIBPANDABSP
#define EXPCL_PANDABSP EXPORT_CLASS
#define EXPTP_PANDABSP EXPORT_TEMPL
#else
#define EXPCL_PANDABSP IMPORT_CLASS
#define EXPTP_PANDABSP IMPORT_TEMPL
#endif

//

ConfigureDecl( config_bsp, EXPCL_PANDABSP, EXPTP_PANDABSP );

#define TypeDecl2(classname, parentname1, parentname2)\
private:\
    static TypeHandle _type_handle;\
public:\
  static TypeHandle get_class_type() {\
    return _type_handle;\
  }\
  static void init_type() {\
    parentname1::init_type();\
    parentname2::init_type();\
    register_type(_type_handle, #classname,\
                  parentname1::get_class_type(),\
		  parentname2::get_class_type());\
  }\
  virtual TypeHandle get_type() const {\
    return classname::get_class_type();\
  }\
  virtual TypeHandle force_init_type() { init_type(); return get_class_type(); }

#define TypeDecl(classname, parentname)\
private:\
    static TypeHandle _type_handle;\
public:\
  static TypeHandle get_class_type() {\
    return _type_handle;\
  }\
  static void init_type() {\
    parentname::init_type();\
    register_type(_type_handle, #classname,\
                  parentname::get_class_type());\
  }\
  virtual TypeHandle get_type() const {\
    return classname::get_class_type();\
  }\
  virtual TypeHandle force_init_type() { init_type(); return get_class_type(); }

#define TypeDef(classname)\
TypeHandle classname::_type_handle;

#ifndef CPPPARSER
extern EXPCL_PANDABSP void init_libpandabsp();
#endif

#endif // CONFIG_BSP_H