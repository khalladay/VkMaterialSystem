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

	Rendering::DrawCall* drawCalls;

	void init()
	{
		Rendering::init();
		//Texture::make("../data/textures/test_texture.jpg");
		Material::initGlobalShaderData();
		uint32_t fruits = Texture::make("../data/textures/fruits.png");
		matId = Material::make("../data/materials/raymarch_primitives.mat");

		Material::setTexture(matId, "testSampler", fruits);

		drawCalls = (Rendering::DrawCall*)malloc(sizeof(Rendering::DrawCall) * 100);
		uint32_t dc = 0;
		for (uint32_t i = 0; i < 10; ++i)
		{
			for (uint32_t j = 0; j < 10; ++j)
			{
				drawCalls[dc].meshIdx = Mesh::quad(0.2, 0.2, -0.9 + i * 0.2f, -0.9 + j * 0.2f);
				drawCalls[dc++].matIdx = matId;
			}
		}

		printf("Total allocation count: %i\n", vkh::GContext.allocator.numAllocs());
	}

	void tick(float deltaTime)
	{
		Rendering::draw(drawCalls, 100);
	}

	void kill()
	{
		
	}
}