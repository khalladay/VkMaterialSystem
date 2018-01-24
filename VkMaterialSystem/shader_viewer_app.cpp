#include "stdafx.h"

#include "shader_viewer_app.h"
#include "rendering.h"
#include "material.h"
#include "mesh.h"
#include "procedural_geo.h"
#include "material_creation.h"
#include "texture.h"
#include "vkh.h"
#include "rendering.h"

namespace App
{
	const int NUM_DRAWCALLS = 1;

	MaterialInstance mInstance;
	DrawCall* drawCalls;

	void init()
	{
		Rendering::init();

		Texture::loadDefaultTexture();
		Material::initGlobalShaderData();
		
		uint32_t fruits = Texture::make("../data/textures/fruits.png");
		mInstance = Material::make("../data/materials/show_uvs.mat");

		
		mInstance = Material::makeInstance(Material::loadInstance("../data/instances/red_tint.inst"));
		

		drawCalls = (DrawCall*)malloc(sizeof(DrawCall) * NUM_DRAWCALLS);
		uint32_t dc = 0;
		
		if (NUM_DRAWCALLS > 1)
		{
			for (uint32_t i = 0; i < 5; ++i)
			{
				for (uint32_t j = 0; j < 5; ++j)
				{
					drawCalls[dc].meshIdx = Mesh::quad(0.4, 0.4, -0.8 + i * 0.4f, -0.8 + j * 0.4f);
					drawCalls[dc++].mat = Material::duplicateInstance(mInstance);

					if (j == i && i == 1)
					{
						Material::setUniformVector4(drawCalls[dc - 1].mat, "tint", glm::vec4(0, 0, 1, 0));
					}
				}
			}
		}
		else
		{
			drawCalls[dc].meshIdx = Mesh::quad(2.0f, 2.0f);
			drawCalls[dc++].mat = Material::duplicateInstance(mInstance);
		}


		printf("Total allocation count: %i\n", vkh::GContext.allocator.numAllocs());
	}

	void tick(float deltaTime)
	{
		Rendering::draw(drawCalls, NUM_DRAWCALLS);
	}

	void kill()
	{
		free(drawCalls);
	}
}