#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "material_loading.h"

namespace App
{
	void init()
	{
		Rendering::init();

		MaterialDefinition def = { 0 };
		
		/*{
			def.fShaderPath = "../data/shaders/fragment_passthrough.frag.spv";
			def.vShaderPath = "../data/shaders/vertex_uvs.vert.spv";
			def.depthTest = false;
			def.depthWrite = true;
			Material::make(def);
		}
*/
		{
			def = Material::load("../data/materials/show_uvs.mat");
			Material::make(def);

		}
		Mesh::quad();
		
	}

	void tick(float deltaTime)
	{
		Rendering::draw();
	}

	void kill()
	{
		
	}
}