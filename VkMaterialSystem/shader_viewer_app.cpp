#include "stdafx.h"

#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "texture.h"
#include "vkh.h"

namespace App
{
	Material::Instance matId = {};

	Rendering::DrawCall* drawCalls;

	void init()
	{
		Rendering::init();

		Material::initGlobalShaderData();
		uint32_t fruits = Texture::make("../data/textures/fruits.png");
		matId = Material::make("../data/materials/show_uvs.mat");

		Material::setTexture(matId, "testSampler", fruits);

		drawCalls = (Rendering::DrawCall*)malloc(sizeof(Rendering::DrawCall) * 25);
		uint32_t dc = 0;
		for (uint32_t i = 0; i < 5; ++i)
		{
			for (uint32_t j = 0; j < 5; ++j)
			{
				drawCalls[dc].meshIdx = Mesh::quad(0.4, 0.4, -0.8 + i * 0.4f, -0.8 + j * 0.4f);
				drawCalls[dc++].mat = Material::make(matId);
			}
		}

		printf("Total allocation count: %i\n", vkh::GContext.allocator.numAllocs());
	}

	void tick(float deltaTime)
	{
		Rendering::draw(drawCalls, 25);
	}

	void kill()
	{
		
	}
}