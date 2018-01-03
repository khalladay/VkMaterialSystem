#include "stdafx.h"

#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "material_creation.h"
#include "texture.h"
#include "vkh.h"
namespace App
{
	uint32_t matId = 0;

	void init()
	{
		Rendering::init();
		//Texture::make("../data/textures/test_texture.jpg");
		Material::initGlobalShaderData();
		uint32_t fruits = Texture::make("../data/textures/fruits.png");
		matId = Material::make("../data/materials/show_uvs.mat");

		Material::loadInstance("../data/instances/red_tint.inst");

		Material::setTexture(matId, "testSampler", fruits);

		Mesh::quad(2.0f, 2.0f);

		printf("Total allocation count: %i\n", vkh::GContext.allocator.numAllocs());
	}

	void tick(float deltaTime)
	{
		Rendering::draw(matId);
	}

	void kill()
	{
		
	}
}