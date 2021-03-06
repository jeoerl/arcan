/*
 * Copyright 2012-2015, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */

#ifndef _HAVE_ARCAN_3DBASE
#define _HAVE_ARCAN_3DBASE

/* Loads a compressed triangle mesh and wrap it around a
 video object */
void arcan_3d_setdefaults();

arcan_errc arcan_3d_camtag(arcan_vobj_id parent,
	float near, float far, float ar, float fov, bool front, bool back);
arcan_vobj_id arcan_3d_buildplane(float minx, float minz, float maxx,
	float maxz, float y, float wdens, float ddens, size_t nmaps);
arcan_vobj_id arcan_3d_buildbox(float width, float height,
	float depth, size_t nmaps);

arcan_errc arcan_3d_swizzlemodel(arcan_vobj_id model);

void arcan_3d_viewray(arcan_vobj_id camtag,
	int x, int y, float fract, vector* pos, vector* ang);

/*
 * The point-cloud generated [count] number of vertices with texture
 * coordinates distributed evenly within a centered horizontal plane.
 *
 * initial vertex positions will be distributed evenly in a
 * horisontal 2D plane with Y set to 0.
 *
 * The principal way for distributing the vertices is to attach
 * an image to a frameset slot (one will be allocated) and use that in a
 * vertex shader that determines the actual position of
 * each vertice.
 */
arcan_vobj_id arcan_3d_pointcloud(size_t count, size_t nmaps);

/*
 * Using the current display settings, take a screen position and
 * determine if, using the view specified by 'cam' a ray from the
 * position would intersect the bounding area of obj
 */
bool arcan_3d_obj_bb_intersect(arcan_vobj_id cam,
	arcan_vobj_id obj, int x, int y);

/* empty model allocates and populates a container,
 * then add a hierarchy of meshes to the model */
arcan_vobj_id arcan_3d_emptymodel();
arcan_errc arcan_3d_finalizemodel(arcan_vobj_id);
arcan_errc arcan_3d_infinitemodel(arcan_vobj_id, bool);
arcan_errc arcan_3d_meshshader(arcan_vobj_id dst,
	agp_shader_id shid, unsigned slot);
arcan_errc arcan_3d_addmesh(arcan_vobj_id dst,
	data_source resource, unsigned nmaps);

/* destructive transform,
 * apply the specified roll / pitch / yaw transform to all
 * the vertices of the model */
arcan_errc arcan_3d_baseorient(arcan_vobj_id dst,
	float roll, float pitch, float yaw);

/* scans through the specified model and all its' meshes,
 * rebuild the bounding volume and using that, maps all vertex values
 * into the -1..1 range */
arcan_errc arcan_3d_scalevertices(arcan_vobj_id vid);

#ifdef A3D_PRIVATE
enum arcan_ffunc_rv arcan_ffunc_3dobj FFUNC_HEAD;
#endif

#endif
