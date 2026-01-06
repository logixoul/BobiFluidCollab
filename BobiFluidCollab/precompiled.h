// dummy file for compatibility with my old projects
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int main(int argc, char** argv);
#undef byte
#endif


#include <glm/glm.hpp>
#include <algorithm>
#include <vector>
#include <functional>
#include <string>
#include <iostream>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <glad/gl.h>
#undef min
#undef max
#undef byte
using namespace glm;
using namespace std;