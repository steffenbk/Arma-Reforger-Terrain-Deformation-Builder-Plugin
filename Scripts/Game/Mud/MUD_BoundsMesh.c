//! Dedicated server: our ground entities have no mesh there, so their bounds are a point and every
//! trace (projectiles, wheel casts, our own checks) skips them and hits the holed heightmap instead
//! (DS run 2026-10-09 20:47, Exp 192624: chunk bounds <1408,0,3584> .. <1408,0,3584>, vobject 0,
//! skin check hit 0 of 2000, blasts 'hit null' at y 0). Two triangles spanning lo..hi (entity local)
//! give the entity real bounds; nothing renders on a dedicated server.
class MUD_BoundsMesh
{
	//------------------------------------------------------------------------------------------------
	//! Sets the mesh on e and returns its Resource (keep a ref, else the mesh goes). null on failure.
	static Resource Make(notnull IEntity e, vector lo, vector hi, string material)
	{
		int numVerts[] = {4};
		int numIndices[] = {6};
		string materials[] = {material};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
			return null;
		// MeshObject takes static arrays (array<T> = compile error)
		vector verts[4];
		verts[0] = Vector(lo[0], lo[1], lo[2]);
		verts[1] = Vector(hi[0], lo[1], lo[2]);
		verts[2] = Vector(lo[0], hi[1], hi[2]);
		verts[3] = Vector(hi[0], hi[1], hi[2]);
		float uvs[] = {0, 0, 1, 0, 0, 1, 1, 1};
		int idx[] = {0, 2, 1, 1, 2, 3};
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);
		e.SetObject(mesh, "");
		e.Update();
		return res;
	}
}
