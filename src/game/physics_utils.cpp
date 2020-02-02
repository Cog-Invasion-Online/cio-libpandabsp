#include "game/physics_utils.h"
#include "game/basegame_shared.h"
#include "game/masks.h"

#include <bulletTriangleMeshShape.h>

pvector<BulletRayHit> ray_test_all_stored( const LPoint3 &from, const LPoint3 &to,
					   const BitMask32 &mask = BitMask32::all_on() )
{
	BulletAllHitsRayResult result = g_game->_physics_world->ray_test_all( from, to, mask );
	pvector<BulletRayHit> sorted_hits;

	if ( result.get_num_hits() > 0 )
	{
		for ( int i = 0; i < result.get_num_hits(); i++ )
		{
			sorted_hits.push_back( result.get_hit( i ) );
		}
		std::sort( sorted_hits.begin(), sorted_hits.end(), []( const BulletRayHit &a, const BulletRayHit &b )
		{
			return ( a.get_hit_fraction() < b.get_hit_fraction() );
		} );
	}
	
	return sorted_hits;
}

void ray_test_closest_not_me( RayTestClosestNotMeResult_t &result, const NodePath &me,
			      const LPoint3 &from, const LPoint3 &to,
			      const BitMask32 &mask )
{
	result.result = false;

	if ( me.is_empty() )
		return;

	pvector<BulletRayHit> hits = ray_test_all_stored( from, to, mask );
	for ( BulletRayHit brh : hits )
	{
		NodePath hitnp( brh.get_node() );
		if ( !me.is_ancestor_of( hitnp ) && me != hitnp )
		{
			result.hit = brh;
			result.result = true;
			return;
		}
	}
}

void optimize_phys( const NodePath &root )
{
	int colls = 0;
	NodePathCollection npc;

	NodePathCollection children = root.get_children();
	for ( int i = 0; i < children.get_num_paths(); i++ )
	{
		NodePath child = children[i];
		if ( child.node()->is_of_type( CollisionNode::get_class_type() ) )
		{
			colls++;
			npc.add_path( child );
		}
	}

	if ( colls > 1 )
	{
		NodePath collide = root.attach_new_node( "__collide__" );
		npc.wrt_reparent_to( collide );
		collide.clear_model_nodes();
		collide.flatten_strong();

		// Move the possibly combined CollisionNodes back to the root.
		collide.get_children().wrt_reparent_to( root );
		collide.remove_node();
	}

	children = root.get_children();
	for ( int i = 0; i < children.get_num_paths(); i++ )
	{
		NodePath child = children[i];
		if ( child.get_name() != "__collide__" )
			optimize_phys( child );
	}
}

void make_bullet_coll_from_panda_coll( const NodePath &root_node, const vector_string &exclusions )
{
	// First combine any redundant CollisionNodes.
	optimize_phys( root_node );

	NodePathCollection npc = root_node.find_all_matches( "**" );
	for ( int i = 0; i < npc.get_num_paths(); i++ )
	{
		NodePath pcollnp = npc[i];
		if ( std::find( exclusions.begin(), exclusions.end(), pcollnp ) != exclusions.end() )
			continue;
		if ( pcollnp.node()->get_type() != CollisionNode::get_class_type() )
			continue;

		CollisionNode *cnode = DCAST( CollisionNode, pcollnp.node() );

		if ( cnode->get_num_solids() == 0 )
			continue;

		PT( BulletBodyNode ) bnode;
		BitMask32 mask = cnode->get_into_collide_mask();
		bool is_ghost = ( !cnode->get_solid( 0 )->is_tangible() );
		if ( is_ghost )
		{
			bnode = new BulletGhostNode( cnode->get_name().c_str() );
			mask = event_mask;
		}
		else
		{
			bnode = new BulletRigidBodyNode( cnode->get_name().c_str() );
		}
		bnode->add_shapes_from_collision_solids( cnode );
		for ( int i = 0; i < bnode->get_num_shapes(); i++ )
		{
			BulletShape *shape = bnode->get_shape( i );
			if ( shape->is_of_type( BulletTriangleMeshShape::get_class_type() ) )
				shape->set_margin( 0.1f );
		}
		bnode->set_kinematic( true );
		NodePath bnp( bnode );
		bnp.reparent_to( pcollnp.get_parent() );
		bnp.set_transform( pcollnp.get_transform() );
		bnp.set_collide_mask( mask );
		// Now that we're using Bullet collisions, we don't need the panda collisions.
		pcollnp.remove_node();
	}
}

void create_and_attach_bullet_nodes( const NodePath &root_node )
{
	make_bullet_coll_from_panda_coll( root_node );
	attach_bullet_nodes( root_node );
}

void attach_bullet_nodes( const NodePath &root_node )
{
	if ( root_node.is_empty() )
		return;

	NodePathCollection npc;
	int i;
	
	npc = root_node.find_all_matches( "**/+BulletRigidBodyNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		g_game->_physics_world->attach( npc[i].node() );
	}
	npc = root_node.find_all_matches( "**/+BulletGhostNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		g_game->_physics_world->attach( npc[i].node() );
	}
}

void detach_bullet_nodes( const NodePath &root_node )
{
	if ( root_node.is_empty() )
		return;

	NodePathCollection npc;
	int i;

	npc = root_node.find_all_matches( "**/+BulletRigidBodyNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		g_game->_physics_world->remove( npc[i].node() );
	}
	npc = root_node.find_all_matches( "**/+BulletGhostNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		g_game->_physics_world->remove( npc[i].node() );
	}
}

void remove_bullet_nodes( const NodePath &root_node )
{
	if ( root_node.is_empty() )
		return;

	NodePathCollection npc;
	int i;

	npc = root_node.find_all_matches( "**/+BulletRigidBodyNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		npc[i].remove_node();
	}
	npc = root_node.find_all_matches( "**/+BulletGhostNode" );
	for ( i = 0; i < npc.get_num_paths(); i++ )
	{
		npc[i].remove_node();
	}
}

void detach_and_remove_bullet_nodes( const NodePath &root_node )
{
	detach_bullet_nodes( root_node );
	remove_bullet_nodes( root_node );
}

LVector3 get_throw_vector( const LPoint3 &trace_origin, const LVector3 &trace_vector,
			   const LPoint3 &throw_origin, const NodePath &me )
{
	LPoint3 trace_end = trace_origin + ( trace_vector * 10000 );
	RayTestClosestNotMeResult_t result;
	ray_test_closest_not_me( result, me,
				 trace_origin,
				 trace_end,
				 WORLD_MASK );
	LPoint3 hit_pos;
	if ( result.result )
		hit_pos = trace_end;
	else
		hit_pos = result.hit.get_hit_pos();

	return ( hit_pos - throw_origin ).normalized();
}