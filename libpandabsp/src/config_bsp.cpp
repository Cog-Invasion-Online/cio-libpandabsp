/**
 * PANDA3D BSP LIBRARY
 * Copyright (c) CIO Team. All rights reserved.
 *
 * @file config_bsp.cpp
 * @author Brian Lach
 * @date March 27, 2018
 */

#include "config_bsp.h"
#include "entity.h"
#include "bsploader.h"
#include "bsp_render.h"
#include "shader_generator.h"
#include "bsp_material.h"
#include "shader_spec.h"

ConfigureDef( config_bsp );
ConfigureFn( config_bsp )
{
        init_libpandabsp();
}

void init_libpandabsp()
{
        static bool initialized = false;
        if ( initialized )
                return;
        initialized = true;

	BSPFaceAttrib::init_type();
	CBaseEntity::init_type();
        CPointEntity::init_type();
	CBrushEntity::init_type();
        CBoundsEntity::init_type();
        BSPRender::init_type();
        BSPCullTraverser::init_type();
        BSPRoot::init_type();
        BSPProp::init_type();
        BSPModel::init_type();
        PSSMShaderGenerator::init_type();

        BSPMaterial::init_type();
        BSPMaterialAttrib::init_type();
        BSPMaterialAttrib::register_with_read_factory();

        ShaderSpec::init_type();
}