#include "precompiled.h"
#include "Array2D_imageProc.h"
#include "util.h"
#include <imgui.h>
#include <imgui-SFML.h>

static bool isMouseEvent(const sf::Event& eve)
{
	if (eve.is<sf::Event::MouseButtonPressed>())
		return true;
	if (eve.is<sf::Event::MouseButtonReleased>())
		return true;
	if (eve.is<sf::Event::MouseMoved>())
		return true;
	if (eve.is<sf::Event::MouseButtonPressed>())
		return true;
	return false;
}

static bool isKeyboardEvent(const sf::Event& eve)
{
	if (eve.is<sf::Event::TextEntered>())
		return true;
	if (eve.is<sf::Event::KeyPressed>())
		return true;
	if (eve.is<sf::Event::KeyReleased>())
		return true;
	
	return false;
}

void disableGLReadClamp() {
	//glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
}
typedef Array2D<float> Image;
int wsx = 800, wsy = 800;
int scale = 3;
float mouseX, mouseY;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
ivec2 sz(sx, sy);
struct Material {
	Array2D<float> img = Array2D<float>(sx, sy);
	Array2D<vec2> tmpEnergy = Array2D<vec2>(sx, sy);
};
Material red, green;
vector<Material*> materials{ &red, &green };
float surfTensionThres;

bool pause = false;

Array2D<float> bounces_dbg;

template<class T, class FetchFunc>
static Array2D<T> gauss3_forwardMapping(Array2D<T> src) {
	T zero = T(0);
	Array2D<T> dst1(src.w, src.h);
	Array2D<T> dst2(src.w, src.h);
	forxy(dst1) {
		dst1(p) = .25f * (2.0f * FetchFunc::fetch(src, p.x, p.y) + get_clamped(src, p.x - 1, p.y) + FetchFunc::fetch(src, p.x + 1, p.y));
	}
	forxy(dst1) {
		//vector<float> weights = { .25, .5, .25 };
		//auto one = T(1);
		FetchFunc::fetch(dst2, p.x, p.y - 1) += .25f * dst1(p);
		FetchFunc::fetch(dst2, p.x, p.y) += .5f * dst1(p);
		FetchFunc::fetch(dst2, p.x, p.y + 1) += .25f * dst1(p);

	}
	return dst2;
}

struct ThisApp {
	struct Config {
		float surfTensionThres = 0.04f;
		float surfTension = 1.0f;
		float gravity = .1f;
		float incompressibilityCoef = 1.0f;
		void update() {
			ImGui::Begin("Config");
			ImGui::DragFloat("surfTensionThres", &surfTensionThres, 1.0f, 0.1f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("surfTension", &surfTension, 1.0f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("gravity", &gravity, 1.0f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::DragFloat("incompressibilityCoef", &incompressibilityCoef, 1.0f, .0001f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::End();

		}
	} mConfig;

	sf::RenderWindow& mWindow;
	bool mLeftMouseButtonHeld = false;
	bool mRightMouseButtonHeld = false;

	ThisApp(sf::RenderWindow* window) : mWindow(*window) {
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


		::mouseX = newPos.x / (float)wsx;
		::mouseY = newPos.y / (float)wsy;

		
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
			std::fill(material->img.begin(), material->img.end(), 0.0f);
			std::fill(material->tmpEnergy.begin(), material->tmpEnergy.end(), vec2());
		}

		for (int x = 0; x < sz.x; x++) {
			for (int y = sz.y * .75; y < sz.y; y++) {
				red.img(x, y) = 1;
			}
		}
	}
	vec2 direction;
	vec2 lastm;
	void draw() {
		mWindow.clear(sf::Color::Black);
		sf::Image toUpload(sf::Vector2u(sx, sy), sf::Color());
		forxy(::red.img) {
			float Lfloat = ::red.img(p);
			Lfloat /= Lfloat + 1.0f;
			unsigned char L = Lfloat * 255;
			toUpload.setPixel(sf::Vector2u(p.x, p.y), sf::Color(L, L, L));
		}

		sf::Texture tex(sf::Vector2u(sx, sy));
		tex.update(toUpload);
		sf::Sprite sprite(tex);
		sprite.setScale(sf::Vector2f(::scale, ::scale));
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
		auto material = &red;

		ivec2 scaledm = ivec2(vec2(mouseX * (float)sx, mouseY * (float)sy));
		if (mLeftMouseButtonHeld)
		{
			//vec2 scaledm = vec2(getMousePos()-getWindow()->getPos()) / float(::scale); //vec2(mouseX * (float)sx, mouseY * (float)sy);

			int r = 80 / ::scale;

			ivec2 areaTopLeft = scaledm - ivec2(r, r);
			ivec2 areaBottomRight = scaledm + ivec2(r, r);

			for (int x = areaTopLeft.x; x <= areaBottomRight.x; x++)
			{
				for (int y = areaTopLeft.y; y <= areaBottomRight.y; y++)
				{
					vec2 v = vec2(x, y) - vec2(scaledm);
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					material->img.wr(x, y) += 1.f * w * 10.0;
				}
			}
		}
		else if (mRightMouseButtonHeld) {
			int r = 80 / ::scale;

			ivec2 areaTopLeft = scaledm - ivec2(r, r);
			ivec2 areaBottomRight = scaledm + ivec2(r, r);

			for (int x = areaTopLeft.x; x <= areaBottomRight.x; x++)
			{
				for (int y = areaTopLeft.y; y <= areaBottomRight.y; y++)
				{
					vec2 v = vec2(x, y) - vec2(scaledm);
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					if (material->img.wr(x, y) != 0.0f)
						material->tmpEnergy.wr(x, y) += w * material->img.wr(x, y) * 4.0f * direction / (float)::scale;
				}
			}
		}
	}

	void doFluidStep() {
		//for (int i = 0; i < 4; i++) {
			//repel(::red, ::green);
			//repel(::green, ::red);
		//}

		for (auto material : ::materials) {
			auto& tmpEnergy = material->tmpEnergy;
			auto& img = material->img;

			forxy(tmpEnergy)
			{
				tmpEnergy(p) += vec2(0.0f, mConfig.gravity) * img(p);
			}

			img = gauss3_forwardMapping<float, WrapModes::GetClamped>(img);
			tmpEnergy = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(tmpEnergy);

			auto img_b = img.clone();
			//for(int i < 0
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
			auto& guidance = img_b;
			forxy(tmpEnergy)
			{
				auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
				if (img_b(p) < surfTensionThres)
				{
					// todo: move the  "* img(p)" back outside the if.
					// todo: readd the safeNormalized()
					//g = safeNormalized(g) * surfTension * img(p);
					g = g * mConfig.surfTension * img(p);
				}
				else
				{
					g *= -mConfig.incompressibilityCoef;
				}

				tmpEnergy(p) += g;
			}

			auto offsets = empty_like(tmpEnergy);
			forxy(offsets) {
				offsets(p) = tmpEnergy(p) / img(p);
			}
			advect(*material, offsets);
		}
	}
	void advect(Material& material, Array2D<vec2> offsets) {
		auto& img = material.img;
		auto& tmpEnergy = material.tmpEnergy;

		auto img3 = Array2D<float>(sx, sy);
		auto tmpEnergy3 = Array2D<vec2>(sx, sy, vec2());
		int count = 0;
		float sumOffsetY = 0; float div = 0;
		forxy(img)
		{
			if (img(p) == 0.0f)
				continue;

			vec2 offset = offsets(p);
			sumOffsetY += abs(offset.y); div++;
			vec2 dst = vec2(p) + offset;

			vec2 newEnergy = tmpEnergy(p);
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
			aaPoint<float, WrapModes::GetClamped>(img3, dst, img(p));
			aaPoint<vec2, WrapModes::GetClamped>(tmpEnergy3, dst, newEnergy);
		}
		//cout << "bugged=" << count << endl;
		//cout << "sumOffsetY=" << sumOffsetY/div << endl;
		img = img3;
		tmpEnergy = tmpEnergy3;
	}
};

int main()
{
    sf::RenderWindow window(sf::VideoMode({ (unsigned int)wsx, (unsigned int)wsy }), "My window");
	window.setFramerateLimit(60);
	if (!ImGui::SFML::Init(window))
		return -1;

	if (!gladLoadGL((GLADloadfunc)sf::Context::getFunction))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	ThisApp app(&window);
	app.setup();
	sf::Clock deltaClock;
	ImGuiIO& io = ImGui::GetIO();
	while (window.isOpen())
    {
		ImGui::SFML::Update(window, deltaClock.restart());

		app.update();
		app.draw();

        while (const std::optional event = window.pollEvent())
        {
			ImGui::SFML::ProcessEvent(window, *event);
			if (ImGui::GetIO().WantCaptureMouse && isMouseEvent(*event))
			{
				continue;
			}

			if (ImGui::GetIO().WantTextInput && isKeyboardEvent(*event))
				continue;

			event->visit(app);
        }
    }
	ImGui::SFML::Shutdown();
}