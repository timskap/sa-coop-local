#pragma once
namespace coop::localinput {
void Install();
bool UsesGamepad();
bool HasInputFocus();
void RegisterWindow(void* window);
}
