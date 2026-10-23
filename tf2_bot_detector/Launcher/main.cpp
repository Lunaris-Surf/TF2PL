#include "../DLLMain.h"

#if defined(WIN32) && !defined(TF2BD_CLI_MAIN)
#include <Windows.h>

// Ask hybrid-graphics laptops (NVIDIA Optimus / AMD PowerXpress) to run us on the discrete GPU.
// These must be exported from the .exe, not the .dll. Direct3D 11 picks its adapter explicitly
// (see the GPU setting); this mainly matters for the OpenGL fallback.
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	return tf2_bot_detector::RunProgram();
}
#else
int main(int argc, const char** argv)
{
	return tf2_bot_detector::RunProgram(argc, argv);
}
#endif
