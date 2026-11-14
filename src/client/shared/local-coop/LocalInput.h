#pragma once
namespace coop::localinput {
void Install();
bool UsesGamepad();
bool HasInputFocus();
void* ContainerWindow();
void RegisterWindow(void* window);
}
