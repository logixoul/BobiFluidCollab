#include "precompiled.h"
#include "Array2D_imageProc.h"
#include "util.h"

void disableGLReadClamp() {
	//glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
}
typedef Array2D<float> Image;
int wsx = 800, wsy = 800;
int scale = 8;
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

/*class Texture2D {
public:
	Texture2D() {
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
	}

	GLuint id;
};*/



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
	void setup()
	{
		disableGLReadClamp();
		reset();
	}
	std::optional<std::string> operator()(const sf::Event::KeyPressed& keyPress)
	{
		// When the enter key is pressed, switch to the next handler type
		if (keyPress.code == sf::Keyboard::Key::R)
		{
			reset();
		}
		if (keyPress.code == sf::Keyboard::Key::P)
		{
			pause = !pause;
		}
		
		return "";
	}

	std::optional<std::string> operator()(const sf::Event::MouseMoved& mouseMoved)
	{
		ivec2 newPos(mouseMoved.position.x, mouseMoved.position.y);


		::mouseX = newPos.x / (float)wsx;
		::mouseY = newPos.y / (float)wsy;

		
		direction = vec2(newPos) - lastm;
		lastm = newPos;
		
		return "";
	}
	template <typename T>
	std::optional<std::string> operator()(const T&)
	{
		// All unhandled events will end up here
		return std::nullopt;
	}
	void reset() {
		std::fill(red.img.begin(), red.img.end(), 0.0f);
		std::fill(red.tmpEnergy.begin(), red.tmpEnergy.end(), vec2());

		std::fill(green.img.begin(), green.img.end(), 0.0f);
		std::fill(green.tmpEnergy.begin(), green.tmpEnergy.end(), vec2());

		for (int x = 0; x < sz.x; x++) {
			for (int y = sz.y * .75; y < sz.y; y++) {
				//red.img(x, y) = 1;
			}
		}
	}
	vec2 direction;
	vec2 lastm;

	void update()
	{
		bounces_dbg = Array2D<float>(sx, sy, 0);
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		//auto material = keys['g'] ? &green : &red;
		auto material = &green;

		ivec2 scaledm = ivec2(vec2(mouseX * (float)sx, mouseY * (float)sy));
		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
		{
			//vec2 scaledm = vec2(getMousePos()-getWindow()->getPos()) / float(::scale); //vec2(mouseX * (float)sx, mouseY * (float)sy);

			int r = 80 / pow(2, ::scale);

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
		else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
			int r = 80 / pow(2, ::scale);

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
		/*surfTensionThres = cfg1::getOpt("surfTensionThres", .04f,
			[&]() { return keys['6']; },
			[&]() { return expRange(mouseY, 0.1f, 50000.0f); });
		auto surfTension = cfg1::getOpt("surfTension", 1.0f,
			[&]() { return keys['7']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto gravity = cfg1::getOpt("gravity", .1f,//0.0f,//.1f,
			[&]() { return keys['8']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto incompressibilityCoef = cfg1::getOpt("incompressibilityCoef", 1.0f,
			[&]() { return keys['/']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });*/
		surfTensionThres = 0.04f;
		auto surfTension = 1.0f;
		auto gravity = .1f;
		auto incompressibilityCoef = 1.0f;


		//for (int i = 0; i < 4; i++) {
			//repel(::red, ::green);
			//repel(::green, ::red);
		//}

		for (auto material : ::materials) {
			auto& tmpEnergy = material->tmpEnergy;
			auto& img = material->img;

			forxy(tmpEnergy)
			{
				tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
			}

			img = gauss3_forwardMapping<float, WrapModes::GetClamped>(img);
			tmpEnergy = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(tmpEnergy);

			auto img_b = img.clone();
			//for(int i < 0
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
			auto& guidance = img_b;
			forxy(tmpEnergy)
			{
				/*if (p.x == tmpEnergy.w - 1)
					continue;
				if (p.y == tmpEnergy.h-1)
					continue;
				if (p.x == 0)
					continue;
				if (p.y == 0)
					continue;*/

					//auto g = gradient_i<float, WrapModes::NoWrap>(guidance, p);
				auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
				if (img_b(p) < surfTensionThres)
				{
					// todo: move the  "* img(p)" back outside the if.
					// todo: readd the safeNormalized()
					//g = safeNormalized(g) * surfTension * img(p);
					g = g * surfTension * img(p);
				}
				else
				{
					g *= -incompressibilityCoef;
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

	if (!gladLoadGL((GLADloadfunc)sf::Context::getFunction))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	ThisApp app;
	app.setup();

	//sf::Image img();
	//sf::Texture tex((void*)::red.img.data, ::red.img.NumBytes());
	sf::Texture tex(sf::Vector2u(::red.img.w, ::red.img.h));


	while (window.isOpen())
    {
		app.update();
		
		window.clear(sf::Color::Black);
		sf::Texture::bind(&tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, tex.getSize().x, tex.getSize().y, 0, GL_RED, GL_FLOAT, ::red.img.data);
		sf::Sprite sprite(tex);
		window.draw(sprite);

		window.display();
		//app.draw();
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
			event->visit(app);
        }
    }
}