#include "precompiled.h"
#include <imgui.h>
#include <imgui-SFML.h>

#include "StefanFluidSketch1.h"

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

int main()
{
    sf::RenderWindow window(sf::VideoMode({ 800, 800 }), "My window");
	window.setFramerateLimit(60);
	if (!ImGui::SFML::Init(window))
		return -1;

	if (!gladLoadGL((GLADloadfunc)sf::Context::getFunction))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	StefanFluidSketch1 app(&window);
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