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

// Optimized 3x3 box blur: uses WrapMode::fetch only for edge pixels. Interior pixels read directly from contiguous memory.
template<class T, class WrapMode>
Array2D<T> boxBlur3x3(Array2D<T> const& in)
{
	int w = in.w;
	int h = in.h;
	Array2D<T> out(w, h);
	if (w <= 0 || h <= 0) return out;

	T zero = ::zero<T>();
	const float inv9 = 1.0f / 9.0f;

	// Helper for edge pixels that need wrap/clamp handling via WrapMode
	auto blurEdge = [&](int x, int y) {
		T sum = zero;
		Array2D<T>& nonConstIn = const_cast<Array2D<T>&>(in);
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				sum += WrapMode::template fetch<T>(nonConstIn, x + dx, y + dy);
			}
		}
		out(x, y) = sum * inv9;
		};

	// If image is too small, compute every pixel via fetch
	if (w < 3 || h < 3) {
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) blurEdge(x, y);
		return out;
	}

	// Top row
	for (int x = 0; x < w; ++x) blurEdge(x, 0);
	// Bottom row
	for (int x = 0; x < w; ++x) blurEdge(x, h - 1);
	// Left and right edges for middle rows
	for (int y = 1; y < h - 1; ++y) {
		blurEdge(0, y);
		blurEdge(w - 1, y);
	}

	// Interior: use direct pointer access for contiguous fast reads
	for (int y = 1; y < h - 1; ++y) {
		T* prevRow = in.data + (y - 1) * w;
		T* curRow = in.data + y * w;
		T* nextRow = in.data + (y + 1) * w;
		for (int x = 1; x < w - 1; ++x) {
			T sum = zero;
			// unrolled 3x3 sum
			sum += prevRow[x - 1]; sum += prevRow[x]; sum += prevRow[x + 1];
			sum += curRow[x - 1]; sum += curRow[x]; sum += curRow[x + 1];
			sum += nextRow[x - 1]; sum += nextRow[x]; sum += nextRow[x + 1];
			out(x, y) = sum * inv9;
		}
	}

	return out;
}


struct Sketch {
	struct Config {
		float surfTensionThres = 0.5f;
		float surfTension = 6.3f;
		float incompressibilityCoef = 1.0f;
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
	vec2 mousePos;
	vec2 prevMousePos;

	const int mScale = 2;
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

		prevMousePos = mousePos;
		mousePos = newPos;
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
		ivec2 scaledm = ivec2(vec2(mousePos) / float(mScale));
		ivec2 prevScaledm = ivec2(vec2(prevMousePos) / float(mScale));
		auto material = manipulateGreen ? &mGreenMaterial : &mRedMaterial;

		if (mLeftMouseButtonHeld || mRightMouseButtonHeld) {
			float value = mLeftMouseButtonHeld ? 1.0 : 0.0;
			for(float f = 0; f <= 1.0; f += .1f)
				paintBlot(material->density, glm::mix(prevScaledm, scaledm, f), value);
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

	void doFluidStep() {
		for (auto material : materials) {
			auto& momentum = material->momentum;
			auto& density = material->density;

			//density = gauss3_forwardMapping<float, WrapModes::GetWrapped>(density);
			density = gaussianBlur<float, WrapModes::GetWrapped>(density, 2 * 2 + 1);
			
			//density = ::boxBlur3x3<float, WrapModes::GetWrapped>(density);
			//density = ::boxBlur3x3<float, WrapModes::GetWrapped>(density);

			//momentum = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(momentum);

			//auto guidance = gaussianBlur<float, WrapModes::GetWrapped>(density, 1 * 2 + 1);
			auto guidance = density.clone();
			auto grads = ::get_gradients<float, WrapModes::GetWrapped>(guidance);
			//auto div = ::get_divergence<float, WrapModes::GetWrapped>(grads);
			//auto guidance = steepConvolve(density);
			forxy(momentum)
			{
				auto g = grads(p);
				auto here = guidance(p);
				if (here < mConfig.surfTensionThres)
				{
					g *= mConfig.surfTension / (here+mConfig.surfTensionThres/100.0);
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
		const auto lowerBound = vec2(0.0f);
		const auto upperBound = vec2(density.Size() - ivec2(2));
		forxy(density)
		{
			vec2 offset = offsets(p);
			vec2 dst = vec2(p) + offset;

			dst = glm::clamp(dst, lowerBound, upperBound);
			aaPoint<float, WrapModes::NoWrap>(density2, dst, density(p));
			//aaPoint<vec2, WrapModes::NoWrap>(momentum2, dst, momentum(p));
		}
		density = density2;
		momentum = momentum2;
	}
	
	static void disableGLReadClamp() {
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
	}
};
