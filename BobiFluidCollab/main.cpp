#include "precompiled.h"
#include <imgui.h>
#include <imgui-SFML.h>

//#include "StefanFluidSketch1.h"
#include "StefanMassPreservingSmoothenerSketch1.h"

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
	//window.setFramerateLimit(60);
	window.setVerticalSyncEnabled(false);
	if (!ImGui::SFML::Init(window))
		return -1;

	if (!gladLoadGL((GLADloadfunc)sf::Context::getFunction))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	Sketch app(&window);
	app.setup();
	sf::Clock deltaClock;
	ImGuiIO& io = ImGui::GetIO();
	float smoothedFps = -1;
	int currentFrame = 0;
	while (window.isOpen())
    {
		sf::Time elapsed = deltaClock.restart();
		ImGui::SFML::Update(window, elapsed);
		const float fps = 1.0f / elapsed.asSeconds();
		if (currentFrame == 2)
			smoothedFps = fps;
		else if(currentFrame >= 3)
			smoothedFps = glm::mix(smoothedFps, fps, .1f);
	
		app.update();

		ImGui::Begin("FPS", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav);
		ImGui::Text("FPS: %.1f", smoothedFps);
		ImGui::End();
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
		currentFrame++;
    }
	ImGui::SFML::Shutdown();
}