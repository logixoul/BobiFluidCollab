#pragma once

#include "Array2D_imageProc.h"
#include "util.h"
#include <imgui.h>
#include "precompiled.h"

template<class T, class Fetch>
Array2D<T> get_divergence(Array2D<vec2>& src) {
	Array2D<T> div(src.Size());
	forxy(div) {
		T dx = (Fetch::template fetch<vec2>(src, p.x + 1, p.y).x - Fetch::template fetch<vec2>(src, p.x - 1, p.y).x) * .5f;
		T dy = (Fetch::template fetch<vec2>(src, p.x, p.y + 1).y - Fetch::template fetch<vec2>(src, p.x, p.y - 1).y) * .5f;
		div(p) = dx + dy;
	}
	return div;
}


struct Sketch {
	struct Config {
		float surfTensionThres = 0.5f;
		float surfTension = 6.3f;
		float incompressibilityCoef = 2.4f;
		float intermaterialRepelCoef = .5f;

		void update() {
			ImGui::Begin("Config");
			ImGui::DragFloat("surfTensionThres", &surfTensionThres, 0.1f, 0.1f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("surfTension", &surfTension, 0.1f, .0001f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("incompressibilityCoef", &incompressibilityCoef, 0.1f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
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
	vector<Material*> materials{ &mRedMaterial /*, &mGreenMaterial*/ };

	bool pause = false;
	bool manipulateGreen = false;

	Array2D<float> bounces_dbg;



	Sketch(sf::RenderWindow* window) : mWindow(*window) {
		sx = window->getSize().x / mScale;
		sy = window->getSize().y / mScale;
		sz = ivec2(sx, sy);

		mRedMaterial = Material(sz);
		mRedMaterial.color = vec3(1.1f, 0.4f, 0.4f);
		mGreenMaterial = Material(sz);
		mGreenMaterial.color = vec3(0.4f, 1.1f, 0.4f);
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
			vec3 totalColor = vec3(0.0f, 0.0f, 0.0f);
			for (Material* material : materials) {
				totalColor += glm::pow(material->color, vec3(1.0f * material->density(p)));
			}
			totalColor = vec3(1.0f) - vec3(1.0f, 1.0f, 1.0f) / totalColor;
			//totalColor /= totalColor + vec3(1.0f);
			totalColor = glm::max(glm::min(totalColor, vec3(1.0f)), vec3(0.0f));
			totalColor *= 255.0f;
			auto totalColorByte = glm::tvec3<unsigned char>(totalColor);
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
	void paintBlot(Array2D<float> dest, ivec2 center, float value) {
		int r = 30 / mScale;
		ivec2 areaTopLeft = center - ivec2(r, r);
		ivec2 areaBottomRight = center + ivec2(r, r);

		for (int x = areaTopLeft.x; x <= areaBottomRight.x; x++)
		{
			for (int y = areaTopLeft.y; y <= areaBottomRight.y; y++)
			{
				vec2 v = vec2(x, y) - vec2(center);
				float w = std::max(0.0f, 1.0f - length(v) / r);
				w = std::min(w, 1.0f);
				w = 3 * w * w - 2 * w * w * w;

				dest.wr(x, y) = mix(dest.wr(x, y), value, w);
			}
		}
	}
	void update()
	{
		mConfig.update();

		bounces_dbg = Array2D<float>(sx, sy, 0);
		if (!pause)
		{
			for(int i = 0; i < 2; i++)
				doFluidStep();

		} // if ! pause
		ivec2 scaledm = ivec2(vec2(mouseX * (float)sx, mouseY * (float)sy));
		auto material = manipulateGreen ? &mGreenMaterial : &mRedMaterial;

		if (mLeftMouseButtonHeld || mRightMouseButtonHeld) {
			float value = mLeftMouseButtonHeld ? 1.0 : 0.0;
			paintBlot(material->density, scaledm, value);
		}
	}

	template<class T, class FetchFunc>
	static Array2D<T> convolve(Array2D<T> in, Array2D<float> kernel) {
		int r = kernel.w / 2;
		auto out = ::empty_like(in);
		forxy(out) {
			float sum = 0.0f;
			for (int kx = -r; kx < r; kx++) {
				for (int ky = -r; ky < r; ky++) {
					sum += kernel(kx + r, ky + r) * FetchFunc::template fetch<T>(in, p.x + kx, p.y + ky);
				}
			}
			out(p) = sum;
		}
		return out;
	}

	Array2D<float> steepConvolve(Array2D<float> in) {
		Array2D<float> kernel(7, 7);
		ivec2 center = kernel.Size() / 2;
		int r = kernel.w / 2;
		forxy(kernel) {
			ivec2 p2 = p - center;
			vec2 p2f = vec2(p2);
			float distance = length(p2f);
			distance = std::min<float>(distance, r);
			//if (distance == 0)
			//	distance = .1;
			kernel(p) = pow(1 - distance / r, 2.0f);
		}
		auto kernelSum = ::accumulate(kernel.begin(), kernel.end(), 0.0f);
		forxy(kernel) {
			kernel(p) /= kernelSum;
		}
		return convolve<float, WrapModes::GetClamped>(in, kernel);
	}

	void repel(Material& affectedMaterial, Material& actingMaterial) {
		auto guidance = steepConvolve(actingMaterial.density);
		//auto guidance = gaussianBlur<float, WrapModes::GetClamped>(actingMaterial.density, 3 * 2 + 1);
		forxy(affectedMaterial.momentum)
		{
			auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
			//if(length(g) != 0.0f) g = glm::normalize(g);

			affectedMaterial.momentum(p) += -g * affectedMaterial.density(p) * mConfig.intermaterialRepelCoef;
		}
	}

	void doFluidStep() {
		//repel(mRedMaterial, mGreenMaterial);
		//repel(mGreenMaterial, mRedMaterial);

		for (auto material : materials) {
			auto& momentum = material->momentum;
			auto& density = material->density;

			density = gauss3_forwardMapping<float, WrapModes::GetWrapped>(density);
			//momentum = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(momentum);

			auto guidance = gaussianBlur<float, WrapModes::GetWrapped>(density, 1 * 2 + 1);
			auto grads = ::get_gradients<float, WrapModes::GetWrapped>(guidance);
			//auto div = ::get_divergence<float, WrapModes::GetWrapped>(grads);
			//auto guidance = steepConvolve(density);
			forxy(momentum)
			{
				auto g = grads(p);
				//if (g == vec2(0.0f, 0.0f))
//					continue;
				auto here = guidance(p);
				/*auto gn = normalize(g);
				auto prev = getBilinear(guidance, vec2(p) - gn);
				auto next = getBilinear(guidance, vec2(p) + gn);
				auto secondPartialDerivative = here - (prev + next) * .5f;*/
				auto pushForce = (here - mConfig.surfTensionThres);
				if (pushForce < 0)
				{
					//float len = length(g);
					//g /= len * len;
					g /= here;
					g *= mConfig.surfTension;
				}
				else
				{
					g *= -mConfig.incompressibilityCoef;
				}

				momentum(p) = g ;
			}

			advect(*material, momentum);
		}
	}
	void advect(Material& material, Array2D<vec2> offsets) {
		auto& density = material.density;
		auto& momentum = material.momentum;

		auto density2 = Array2D<float>(sx, sy);
		auto momentum2 = Array2D<vec2>(sx, sy, vec2());
		int count = 0;
		forxy(density)
		{
			vec2 offset = offsets(p);
			vec2 dst = vec2(p) + offset;

			aaPoint<float, WrapModes::Get_WrapZeros>(density2, dst, density(p));
			aaPoint<vec2, WrapModes::Get_WrapZeros>(momentum2, dst, momentum(p));
		}
		density = density2;
		momentum = momentum2;
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
