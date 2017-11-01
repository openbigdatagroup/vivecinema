/******************************************************************************
 * @file	BLRawAssetVersion.h                                               *
 * @desc    raw asset(output from exporter) data version					  *
 *                                                                            *
 * @author	andre chen                                                        *
 * @history	2010/03/12 version 0.0											  *
 *			2010/04/24 version 0.1, add texcoord cell/scroll animations       *
 *			2010/06/21 version 1.0, calculate tangenet basis handedness       *
 *			2010/09/13 version 2.0, integrate shader material                 *
 *			2010/10/13 version 2.1, calculate vertex normal vector            *
 *			[pending] version 2.2, remove duplicated(instanced) mesh          *
 *                                                                            *
 * Copyright (c) 2010, All Rights Reserved									  *
 ******************************************************************************/
#ifndef BL_RAW_ASSET_DATA_VERSION
#define BL_RAW_ASSET_DATA_VERSION

// model
#define MODEL_VER_MAJOR			(2)
#define MODEL_VER_MINOR			(1)
#define MODEL_FILE_VERSION		((MODEL_VER_MAJOR<<16)|MODEL_VER_MINOR)
#define MODEL_GEOMETRY_MAGIC	(('g'<<24)|('e'<<16)|('o'<<8)|'m')

// scene
#define WORLD_VER_MAJOR			(0)
#define WORLD_VER_MINOR			(0)
#define WORLD_FILE_VERSION		((WORLD_VER_MAJOR<<16)|WORLD_VER_MINOR)
#define WORLD_GEOMETRY_MAGIC	(('w'<<24)|('l'<<16)|('r'<<8)|'d')
#define WORLD_MATERIAL_MAGIC	(('m'<<24)|('a'<<16)|('t'<<8)|'l')
#define WORLD_INSTANCE_MAGIC	(('i'<<24)|('n'<<16)|('s'<<8)|'t')
#define WORLD_LIGHT_MAGIC		(('l'<<24)|('i'<<16)|('t'<<8)|'e')
#define WORLD_SPLINE_MAGIC		(('s'<<24)|('p'<<16)|('l'<<8)|'n')
#define WORLD_CAMERA_MAGIC		(('c'<<24)|('a'<<16)|('m'<<8)|'r')
#define WORLD_DUMMY_MAGIC		(('d'<<24)|('u'<<16)|('m'<<8)|'i')

#endif