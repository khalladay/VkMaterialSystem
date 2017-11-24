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
	uint32_t matId = 0;

	void init()
	{
		Rendering::init();
		//Texture::make("../data/textures/test_texture.jpg");
		Material::initGlobalShaderData();
		uint32_t happy = Texture::make("../data/textures/happy_dog.jpg");
		matId = Material::make("../data/materials/show_uvs.mat");

		Material::setTexture(matId, "testSampler", happy);

		Mesh::quad();

	}

	void tick(float deltaTime)
	{
		Rendering::draw(matId);
	}

	void kill()
	{
		
	}
}