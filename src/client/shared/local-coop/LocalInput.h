#pragma once
namespace coop::localinput {
void Install();
bool UsesGamepad();
bool HasInputFocus();
void* ContainerWindow();
void RequestFocusRecovery();
void ServiceFocus();
void RegisterWindow(void* window);
}
