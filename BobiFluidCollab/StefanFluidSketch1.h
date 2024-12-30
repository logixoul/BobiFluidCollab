#pragma once

#include "Array2D_imageProc.h"
#include "util.h"
#include <imgui.h>
#include "precompiled.h"

struct StefanFluidSketch1 {
	struct Config {
		float surfTensionThres = 2.245f;
		float surfTension = 1.0f;
		float gravity = .1f;
		float incompressibilityCoef = 1.0f;
		float intermaterialRepelCoef = .5f;

		void update() {
			ImGui::Begin("Config");
			ImGui::DragFloat("surfTensionThres", &surfTensionThres, 0.1f, 0.1f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("surfTension", &surfTension, 0.1f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("gravity", &gravity, 0.1f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("incompressibilityCoef", &incompressibilityCoef, 0.1f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("intermaterialRepelCoef", &intermaterialRepelCoef, 0.1f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::End();

		}
	} mConfig;

	sf::RenderWindow& mWindow;
	bool mLeftMouseButtonHeld = false;
	bool mRightMouseButtonHeld = false;
	float mouseX, mouseY;

	const int mScale = 5;
	int sx;
	int sy;
	ivec2 sz;
	struct Material {
		Material() {
		}
		Material(ivec2 size) {
			density = Array2D<float>(size);
			momentum = Array2D<vec2>(size);
		}
		Array2D<float> density;
		Array2D<vec2> momentum;
		vec3 color;
	};
	Material mRedMaterial, mGreenMaterial;
	vector<Material*> materials{ &mRedMaterial, &mGreenMaterial };

	bool pause = false;
	bool manipulateGreen = false;

	Array2D<float> bounces_dbg;



	StefanFluidSketch1(sf::RenderWindow* window) : mWindow(*window) {
		sx = window->getSize().x / mScale;
		sy = window->getSize().y / mScale;
		sz = ivec2(sx, sy);

		mRedMaterial = Material(sz);
		mRedMaterial.color = vec3(1.0f, 0.7f, 0.4f);
		mGreenMaterial = Material(sz);
		mGreenMaterial.color = vec3(0.7f, 1.0f, 0.4f);
		materials = { &mRedMaterial, &mGreenMaterial };
	}
	void setup()
	{
		disableGLReadClamp();
		reset();
	}
	void operator()(const sf::Event::KeyPressed& e)
	{
		if (e.code == sf::Keyboard::Key::R)
		{
			reset();
		}
		if (e.code == sf::Keyboard::Key::P)
		{
			pause = !pause;
		}
		if (e.code == sf::Keyboard::Key::G)
		{
			this->manipulateGreen = !this->manipulateGreen;
		}
	}
	void operator()(const sf::Event::MouseButtonPressed& e) {
		if (e.button == sf::Mouse::Button::Left)
			this->mLeftMouseButtonHeld = true;
		else if (e.button == sf::Mouse::Button::Right)
			this->mRightMouseButtonHeld = true;
	}
	void operator()(const sf::Event::MouseButtonReleased& e) {
		if (e.button == sf::Mouse::Button::Left)
			this->mLeftMouseButtonHeld = false;
		else if (e.button == sf::Mouse::Button::Right)
			this->mRightMouseButtonHeld = false;
	}
	void operator()(const sf::Event::MouseMoved& e)
	{
		ivec2 newPos(e.position.x, e.position.y);


		mouseX = newPos.x / (float)mWindow.getSize().x;
		mouseY = newPos.y / (float)mWindow.getSize().y;


		direction = vec2(newPos) - lastm;
		lastm = newPos;
	}
	void operator()(const sf::Event::Closed& e)
	{
		mWindow.close();
	}
	template <typename T>
	void operator()(const T&)
	{
		// All unhandled events will end up here
	}
	void reset() {
		for (Material* material : materials) {
			std::fill(material->density.begin(), material->density.end(), 0.0f);
			std::fill(material->momentum.begin(), material->momentum.end(), vec2());
		}
	}
	vec2 direction;
	vec2 lastm;
	void draw() {
		mWindow.clear(sf::Color::Black);
		sf::Image toUpload(sf::Vector2u(sx, sy), sf::Color());
		forxy(mRedMaterial.density) {
			vec3 totalColor = vec3(1.0f, 1.0f, 1.0f);
			for (Material* material : materials) {
				totalColor *= glm::pow(material->color, vec3(0.1f*material->density(p)));
			}
			//totalColor /= totalColor + vec3(1.0f);
			totalColor *= 255.0f;
			auto totalColorByte = glm::tvec3<byte>(totalColor);
			toUpload.setPixel(sf::Vector2u(p.x, p.y), sf::Color(totalColorByte.x, totalColorByte.y, totalColorByte.z));
		}

		sf::Texture tex(sf::Vector2u(sx, sy));
		tex.update(toUpload);
		tex.setSmooth(true);
		sf::Sprite sprite(tex);
		sprite.setScale(sf::Vector2f(mScale, mScale));
		mWindow.draw(sprite);
		ImGui::SFML::Render(mWindow);
		mWindow.display();
	}
	void update()
	{
		mConfig.update();

		bounces_dbg = Array2D<float>(sx, sy, 0);
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		//auto material = keys['g'] ? &green : &red;
		auto material = manipulateGreen ? &mGreenMaterial : &mRedMaterial;

		ivec2 scaledm = ivec2(vec2(mouseX * (float)sx, mouseY * (float)sy));
		int r = 80 / mScale;
		ivec2 areaTopLeft = scaledm - ivec2(r, r);
		ivec2 areaBottomRight = scaledm + ivec2(r, r);

		if (mLeftMouseButtonHeld || mRightMouseButtonHeld) {
			for (int x = areaTopLeft.x; x <= areaBottomRight.x; x++)
			{
				for (int y = areaTopLeft.y; y <= areaBottomRight.y; y++)
				{
					vec2 v = vec2(x, y) - vec2(scaledm);
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					
					if (mLeftMouseButtonHeld) {
						material->density.wr(x, y) += 1.f * w * 10.0;
					}
					else if (mRightMouseButtonHeld) {
						if (material->density.wr(x, y) != 0.0f)
							material->momentum.wr(x, y) += w * material->density.wr(x, y) * 0.5f * direction / (float)mScale;
					}
				}
			}
		}
	}

	void repel(Material& affectedMaterial, Material& actingMaterial) {
		auto img_b = actingMaterial.density.clone();
		img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
		auto& guidance = img_b;
		forxy(affectedMaterial.momentum)
		{
			auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
			
			affectedMaterial.momentum(p) += -g * affectedMaterial.density(p) * mConfig.intermaterialRepelCoef;
		}

		/*auto offsets = empty_like(momentum);
		forxy(offsets) {
			offsets(p) = momentum(p) / density(p);
		}
		advect(*material, offsets);*/
	}

	void doFluidStep() {
		//for (int i = 0; i < 4; i++) {
			repel(mRedMaterial, mGreenMaterial);
			repel(mGreenMaterial, mRedMaterial);
		//}

		for (auto material : materials) {
			auto& momentum = material->momentum;
			auto& density = material->density;

			forxy(momentum)
			{
				momentum(p) += vec2(0.0f, mConfig.gravity) * density(p);
			}

			density = gauss3_forwardMapping<float, WrapModes::GetClamped>(density);
			momentum = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(momentum);

			auto img_b = density.clone();
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
			auto& guidance = img_b;
			forxy(momentum)
			{
				auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
				if (img_b(p) < mConfig.surfTensionThres)
				{
					// todo: move the  "* density(p)" back outside the if.
					// todo: readd the safeNormalized()
					//g = safeNormalized(g) * surfTension * density(p);
					g = g * mConfig.surfTension * density(p);
				}
				else
				{
					g *= -mConfig.incompressibilityCoef;
				}

				momentum(p) += g;
			}

			auto offsets = empty_like(momentum);
			forxy(offsets) {
				offsets(p) = momentum(p) / density(p);
			}
			advect(*material, offsets);
		}
	}
	void advect(Material& material, Array2D<vec2> offsets) {
		auto& density = material.density;
		auto& momentum = material.momentum;

		auto img3 = Array2D<float>(sx, sy);
		auto momentum3 = Array2D<vec2>(sx, sy, vec2());
		int count = 0;
		float sumOffsetY = 0; float div = 0;
		forxy(density)
		{
			if (density(p) == 0.0f)
				continue;

			vec2 offset = offsets(p);
			sumOffsetY += abs(offset.y); div++;
			vec2 dst = vec2(p) + offset;

			vec2 newEnergy = momentum(p);
			bool bounced = false;
			for (int dim = 0; dim <= 1; dim++) {
				float maxVal = sz[dim] - 1;
				if (dst[dim] > maxVal) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = maxVal - (dst[dim] - maxVal);
					//if(dim==1)
						//cout << "dst[dim]=" << dst[dim] << endl;
					bounced = true;
				}
				if (dst[dim] < 0) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = -dst[dim];
					bounced = true;
				}
			}
			if (dst.y >= sz.y - 1)
				count++;
			//if(bounced)
			//	aaPoint<float, WrapModes::NoWrap>(bounces_dbg, dst, 1);
			aaPoint<float, WrapModes::GetClamped>(img3, dst, density(p));
			aaPoint<vec2, WrapModes::GetClamped>(momentum3, dst, newEnergy);
		}
		//cout << "bugged=" << count << endl;
		//cout << "sumOffsetY=" << sumOffsetY/div << endl;
		density = img3;
		momentum = momentum3;
	}
	template<class T, class FetchFunc>
	static Array2D<T> gauss3_forwardMapping(Array2D<T> src) {
		T zero = T(0);
		Array2D<T> dst1(src.w, src.h);
		Array2D<T> dst2(src.w, src.h);
		forxy(dst1) {
			dst1(p) = .25f * (2.0f * FetchFunc::fetch(src, p.x, p.y) + get_clamped(src, p.x - 1, p.y) + FetchFunc::fetch(src, p.x + 1, p.y));
		}
		forxy(dst1) {
			FetchFunc::fetch(dst2, p.x, p.y - 1) += .25f * dst1(p);
			FetchFunc::fetch(dst2, p.x, p.y) += .5f * dst1(p);
			FetchFunc::fetch(dst2, p.x, p.y + 1) += .25f * dst1(p);
		}
		return dst2;
	}

	static void disableGLReadClamp() {
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
	}
};