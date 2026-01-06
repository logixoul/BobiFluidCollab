#pragma once

#include "Array2D_imageProc.h"
#include "util.h"
#include <imgui.h>
#include "precompiled.h"

sf::Color hsv(int hue, float sat, float val)
{
	hue %= 360;
	while (hue < 0) hue += 360;

	if (sat < 0.f) sat = 0.f;
	if (sat > 1.f) sat = 1.f;

	if (val < 0.f) val = 0.f;
	if (val > 1.f) val = 1.f;

	int h = hue / 60;
	float f = float(hue) / 60 - h;
	float p = val * (1.f - sat);
	float q = val * (1.f - sat * f);
	float t = val * (1.f - sat * (1 - f));

	switch (h)
	{
	default:
	case 0:
	case 6: return sf::Color(val * 255, t * 255, p * 255);
	case 1: return sf::Color(q * 255, val * 255, p * 255);
	case 2: return sf::Color(p * 255, val * 255, t * 255);
	case 3: return sf::Color(p * 255, q * 255, val * 255);
	case 4: return sf::Color(t * 255, p * 255, val * 255);
	case 5: return sf::Color(val * 255, p * 255, q * 255);
	}
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

typedef glm::vec3 Cell;

struct Sketch {
	struct Config {
		float surfTensionThres = 0.293f;
		float surfTension = 6.3f;
		float incompressibilityCoef = 1.0f;

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

	const int mScale = 4;
	Array2D<Cell> density;
	
	bool pause = false;

	Sketch(sf::RenderWindow* window) : mWindow(*window) {
		int sx = window->getSize().x / mScale;
		int sy = window->getSize().y / mScale;
		density = Array2D<Cell>(sx, sy);
	}
	void setup()
	{
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
		std::fill(density.begin(), density.end(), Cell(0.0f));
	}
	void draw() {
		mWindow.clear(sf::Color::Black);
		sf::Image toUpload(sf::Vector2u(density.w, density.h), sf::Color());
		forxy(density) {
			vec3 totalColor = density(p);
			totalColor = glm::max(glm::min(totalColor, vec3(1.0f)), vec3(0.0f));
			totalColor *= 255.0f;
			auto totalColorByte = glm::tvec3<unsigned char>(totalColor);
			toUpload.setPixel(sf::Vector2u(p.x, p.y), sf::Color(totalColorByte.x, totalColorByte.y, totalColorByte.z));
		}

		sf::Texture tex(toUpload.getSize());
		tex.update(toUpload);
		tex.setSmooth(true);
		sf::Sprite sprite(tex);
		sprite.setScale(sf::Vector2f(mScale, mScale));
		mWindow.draw(sprite);
		ImGui::SFML::Render(mWindow);
		mWindow.display();
	}
	void paintBlot(Array2D<Cell> dest, ivec2 center, Cell value) {
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

				dest.wr(x, y) = glm::mix(dest.wr(x, y), value, w);
			}
		}
	}
	void update()
	{
		mConfig.update();

		if (!pause)
		{
			//for(int i = 0; i < 2; i++)
				doFluidStep();

		} // if ! pause
		ivec2 scaledm = ivec2(vec2(mousePos) / float(mScale));
		ivec2 prevScaledm = ivec2(vec2(prevMousePos) / float(mScale));
		
		static float hue = 0;
		if (mLeftMouseButtonHeld || mRightMouseButtonHeld) {
			if (mLeftMouseButtonHeld)
				hue += glm::distance<float>(vec2(scaledm), vec2(prevScaledm));
			auto colorToPut = ::hsv(hue, 1.0, .5);
			Cell cellToPut = Cell(colorToPut.r, colorToPut.g, colorToPut.b)/255.0f;
			cellToPut /= glm::dot(cellToPut, vec3(1.0 / 3.0));
			Cell value = mLeftMouseButtonHeld ? cellToPut : Cell(0.0);
			for(float f = 0; f <= 1.0; f += .1f)
				paintBlot(density, glm::mix(prevScaledm, scaledm, f), value*.4f);
		}
	}

	
	void doFluidStep() {
			Array2D<vec2> momentum(density.Size());

			density = gaussianBlur<Cell, WrapModes::GetWrapped>(density, 4 * 2 + 1);
			Array2D<float> densityMono(density.Size());
			forxy(density) {
				densityMono(p) = glm::dot(density(p), vec3(1.0/3.0));
			}
			auto guidance = gaussianBlur<float, WrapModes::GetWrapped>(densityMono, 4 * 2 + 1);
			
			auto grads = ::get_gradients<float, WrapModes::GetWrapped>(guidance);
			forxy(momentum)
			{
				auto g = grads(p);
				auto here = densityMono(p);
				if (here < mConfig.surfTensionThres)
				{
					g *= mConfig.surfTension / (here+mConfig.surfTensionThres/10.0);
				}
				momentum(p) = g ;
			}

			auto density2 = empty_like(density);
			int count = 0;
			//const auto lowerBound = vec2(0.0f);
			//const auto upperBound = vec2(density.Size() - ivec2(2));
			forxy(density)
			{
				float hereMono = densityMono(p);
				vec2 offset = momentum(p);
				vec2 dst;
				do {
					dst = vec2(p) + offset;

					//dst = glm::clamp(dst, lowerBound, upperBound);
					const float atDst = getBilinear(densityMono, dst);
					if (hereMono < mConfig.surfTensionThres && atDst > mConfig.surfTensionThres)
						offset *= .9f;
					else if (hereMono > mConfig.surfTensionThres && atDst < mConfig.surfTensionThres)
						offset *= .9f;
					else
						break;
				} while (true);
				aaPoint<Cell, WrapModes::WrapModes::GetWrapped>(density2, dst, density(p));
			}
			density = density2;
	}
};
