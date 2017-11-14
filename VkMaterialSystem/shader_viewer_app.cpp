#include "stdafx.h"

#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "material_creation.h"
#include "texture.h"
namespace App
{
	void init()
	{
		Rendering::init();
		//Texture::make("../data/textures/test_texture.jpg");

		Material::Definition def = { 0 };		
		{
			Material::make("../data/materials/raymarch_primitives.mat");
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