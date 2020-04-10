/**
 * PANDA3D BSP LIBRARY
 *
 * Copyright (c) Brian Lach <brianlach72@gmail.com>
 * All rights reserved.
 *
 * @file c_baseentity.cpp
 * @author Brian Lach
 * @date January 25, 2020
 */

#include "c_baseentity.h"
#include "clientbase.h"
#include "cl_entitymanager.h"
#include "cl_rendersystem.h"
#include "cl_netinterface.h"
#include <configVariableBool.h>

#include <modelRoot.h>
#include <pStatCollector.h>
#include <pStatTimer.h>

NotifyCategoryDef( c_baseentity, "" )

//
// Interpolation
//
// Entity has list of CInterpolatedVars mapped to their actual variable.
// 
// Each update, if the network time has changed, each interpolated var is marked as changed.
// If the value on the interpolated var has changed since the last update, the var is
// marked as needing interpolation.
//
// If the entity has any variables needing interpolation, it is added to a global list of entities
// that need interpolation.
//
// After receiving the latest server snapshot and updating all entities, all entities in the
// interpolation list will have interpolate() called on them.
//

static ConfigVariableBool cl_interp_all( "cl_interp_all", true );

static PStatCollector collect_interp_collector( "Entity:CollectInterpolateVars" );
static PStatCollector interp_collector( "Entity:Interpolate" );

static pvector<C_BaseEntity *> g_interpolationlist;
static pvector<C_BaseEntity *> g_teleportlist;

C_BaseEntity::C_BaseEntity() :
	_bsp_entnum( -1 ),
	_simulation_time( 0.0f ),
	_simulation_tick( 0 ),
	_old_simulation_time( 0.0f ),
	_interpolation_list_entry( 0xFFFF ),
	_teleport_list_entry( 0xFFFF ),
	_owner_entity( 0 ),
	_iv_origin( "C_BaseEntity_iv_origin" ),
	_iv_angles( "C_BaseEntity_iv_angles" ),
	_iv_scale( "C_BaseEntity_iv_scale" )
{
	add_var( &_origin, &_iv_origin, LATCH_SIMULATION_VAR );
	add_var( &_angles, &_iv_angles, LATCH_SIMULATION_VAR );
	add_var( &_scale, &_iv_scale, LATCH_SIMULATION_VAR );
}

void C_BaseEntity::init( entid_t entnum )
{
	_entnum = entnum;

	interp_setup_mappings( get_var_mapping() );
}

void C_BaseEntity::spawn()
{
	CBaseEntityShared::spawn();
	update_parent_entity( get_parent_entity() );
}





//
// Interpolation
//

void C_BaseEntity::add_to_teleport_list()
{
	if ( _teleport_list_entry == 0xFFFF )
	{
		g_teleportlist.push_back( this );
		_teleport_list_entry = g_teleportlist.size() - 1;
	}
}

void C_BaseEntity::remove_from_teleport_list()
{
	if ( _teleport_list_entry != 0xFFFF )
	{
		g_teleportlist.erase( g_teleportlist.begin() + _teleport_list_entry );
		_teleport_list_entry = 0xFFFF;
	}
}

void C_BaseEntity::add_to_interpolation_list()
{
	if ( _interpolation_list_entry == 0xFFFF )
	{
		c_baseentity_cat.debug()
			<< "Adding " << get_entnum() << " to interpolation list" << std::endl;
		g_interpolationlist.push_back( this );
		_interpolation_list_entry = g_interpolationlist.size() - 1;
	}
}

void C_BaseEntity::remove_from_interpolation_list()
{
	if ( _interpolation_list_entry != 0xFFFF )
	{
		c_baseentity_cat.debug()
			<< "Removing " << get_entnum() << " from interpolation list" << std::endl;
		g_interpolationlist.erase( std::find( g_interpolationlist.begin(), g_interpolationlist.end(), this ) );
		_interpolation_list_entry = 0xFFFF;
	}
}

float C_BaseEntity::get_last_changed_time( int flags )
{
	if ( flags & LATCH_SIMULATION_VAR )
	{
		if ( _simulation_time == 0.0 )
		{
			return cl->get_curtime();
		}
		return _simulation_time;
	}

	assert( 0 );
	return cl->get_curtime();
}

void C_BaseEntity::on_store_last_networked_value()
{
	PStatTimer timer( collect_interp_collector );

	size_t c = _var_map.m_Entries.size();
	for ( size_t i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &_var_map.m_Entries[i];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		watcher->NoteLastNetworkedValue();
	}
}

void C_BaseEntity::on_latch_interpolated_vars( int flags )
{
	PStatTimer timer( collect_interp_collector );

	c_baseentity_cat.debug()
		<< "OnLatchInterpolatedVars" << std::endl;
	float changetime = get_last_changed_time( flags );
	//std::cout << "network local changetime: " << changetime << std::endl;
	//std::cout << "currtime: " << CClockDelta::get_global_ptr()->get_local_network_time() << std::endl;


	bool update_last = ( flags & INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED ) == 0;

	size_t c = _var_map.m_Entries.size();
	for ( size_t i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &_var_map.m_Entries[i];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( !( type & flags ) )
			continue;

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		if ( watcher->NoteChanged( changetime, update_last ) )
		{
			e->m_bNeedsToInterpolate = true;
		}
			
	}

	if ( should_interpolate() )
		add_to_interpolation_list();
}

bool C_BaseEntity::interpolate( float curr_time )
{
	c_baseentity_cat.debug()
		<< "interpolating " << get_entnum() << std::endl;

	LVector3 old_origin;
	LVector3 old_angles;

	int done;
	int retval =
		base_interpolate_1( curr_time, old_origin, old_angles, done );

	if ( done )
		remove_from_interpolation_list();

	return true;
}

int C_BaseEntity::base_interpolate_1( float &curr_time, LVector3 &old_org,
				      LVector3 &old_ang, int &done )
{
	// Don't mess with the world!!!
	done = 1;

	// These get moved to the parent position automatically

	old_org = _origin;
	old_ang = _angles;

	done = interp_interpolate( get_var_mapping(), curr_time );

	return done;
}

int C_BaseEntity::interp_interpolate( VarMapping_t *map, float curr_time )
{
	bool done = true;
	if ( curr_time < map->m_lastInterpolationTime )
	{
		for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
		{
			VarMapEntry_t *e = &map->m_Entries[i];
			e->m_bNeedsToInterpolate = true;
		}
	}
	map->m_lastInterpolationTime = curr_time;

	for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[i];

		if ( !e->m_bNeedsToInterpolate )
			continue;

		IInterpolatedVar *watcher = e->watcher;
		assert( !( watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE ) );

		if ( watcher->Interpolate( curr_time ) )
			e->m_bNeedsToInterpolate = false;
		else
			done = false;
	}

	return done;
}

bool C_BaseEntity::should_interpolate()
{
	return true;
}

void C_BaseEntity::post_data_update()
{
	bool simulation_changed = _simulation_time != _old_simulation_time;
	if ( !is_predictable() )
	{
		if ( simulation_changed )
		{
			// Update interpolated simulation vars
			on_latch_interpolated_vars( LATCH_SIMULATION_VAR );
		}
	}
	else
	{
		// Just store off the last networked value for use in prediction
		on_store_last_networked_value();
	}

	_old_simulation_time = _simulation_time;
}

void C_BaseEntity::interp_setup_mappings( VarMapping_t *map )
{
	if ( !map )
		return;

	size_t c = map->m_Entries.size();
	for ( size_t i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[i];
		IInterpolatedVar *watcher = e->watcher;
		void *data = e->data;
		int type = e->type;

		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( get_interpolate_amount( watcher->GetType() ) );

	}
}

void C_BaseEntity::add_var( void *data, IInterpolatedVar *watcher, int type,
			    bool setup )
{
	// Only add it if it hasn't been added yet.
	bool bAddIt = true;
	for ( size_t i = 0; i < _var_map.m_Entries.size(); i++ )
	{
		if ( _var_map.m_Entries[i].watcher == watcher )
		{
			if ( ( type & EXCLUDE_AUTO_INTERPOLATE ) !=
				( watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE ) )
			{
				// Its interpolation mode changed, so get rid of it and re-add it.
				remove_var( _var_map.m_Entries[i].data, true );
			}
			else
			{
				// They're adding something that's already there. No need to re-add it.
				bAddIt = false;
			}

			break;
		}
	}

	if ( bAddIt )
	{
		// watchers must have a debug name set
		nassertv( watcher->GetDebugName() != NULL );

		VarMapEntry_t map;
		map.data = data;
		map.watcher = watcher;
		map.type = type;
		map.m_bNeedsToInterpolate = true;
		if ( type & EXCLUDE_AUTO_INTERPOLATE )
		{
			_var_map.m_Entries.push_back( map );
		}
		else
		{
			_var_map.m_Entries.insert( _var_map.m_Entries.begin(), map );
			++_var_map.m_nInterpolatedEntries;
		}
	}

	if ( setup )
	{
		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( get_interpolate_amount( watcher->GetType() ) );
	}
}

void C_BaseEntity::remove_var( void *data, bool bAssert )
{
	for ( size_t i = 0; i < _var_map.m_Entries.size(); i++ )
	{
		if ( _var_map.m_Entries[i].data == data )
		{
			if ( !( _var_map.m_Entries[i].type & EXCLUDE_AUTO_INTERPOLATE ) )
				--_var_map.m_nInterpolatedEntries;

			_var_map.m_Entries.erase( _var_map.m_Entries.begin() + i );
			return;
		}
	}
}

float C_BaseEntity::get_interpolate_amount( int flags )
{
	int server_tick_multiple = 0;
	return cl->ticks_to_time( cl->time_to_ticks( get_client_interp_amount() ) + server_tick_multiple );
}

VarMapping_t *C_BaseEntity::get_var_mapping()
{
	return &_var_map;
}

void C_BaseEntity::interpolate_entities()
{
	PStatTimer timer( interp_collector );

	c_baseentity_cat.debug()
		<< "Interpolating " << g_interpolationlist.size() << " entities\n";

	for ( size_t i = 0; i < g_interpolationlist.size(); i++ )
	{
		C_BaseEntity *ent = g_interpolationlist[i];
		ent->interpolate( cl->get_curtime() );
		ent->post_interpolate();
	}
}

void C_BaseEntity::post_interpolate()
{
	// Apply interpolated transform to node
	if ( !_np.is_empty() )
	{
		_np.set_pos( _origin );
		_np.set_hpr( _angles );
		_np.set_scale( _scale );
	}

	c_baseentity_cat.debug()
		<< "Post interpolate " << get_entnum() << ":\n"
	        << "\tPos: " << _origin << "\n"
		<< "\tAngles: " << _angles << "\n";
}

bool C_BaseEntity::is_predictable()
{
	return false;
}

void C_BaseEntity::send_entity_message( Datagram &dg )
{
	clnet->send_datagram( dg );
}

void C_BaseEntity::receive_entity_message( int msgtype, DatagramIterator &dgi )
{
}

IMPLEMENT_CLIENTCLASS_RT_NOBASE( C_BaseEntity, CBaseEntity )
	RecvPropInt( PROPINFO( _bsp_entnum ) ),
	RecvPropInt( PROPINFO( _simulation_tick ) ),
	RecvPropEntnum( PROPINFO( _owner_entity ) )
END_RECV_TABLE()