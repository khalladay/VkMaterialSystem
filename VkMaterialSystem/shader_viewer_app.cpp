#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "material_loading.h"
#include "texture.h"

namespace App
{
	void init()
	{
		Rendering::init();
		//Texture::make("../data/textures/test_texture.jpg");

		MaterialDefinition def = { 0 };		
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