#include <vk_engine.h>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[])
{
	LunaticEngine engine;

	engine.Init();	
	
	engine.Run();	

	engine.Cleanup();
#if 0
	std::string input;
	std::thread runThread;
	LunaticEngine* engine = nullptr;
	bool bIsRunning = false;
	bool bIsInitialized = false;
	while (true)
	{
		std::getline(std::cin, input);

		std::string lowered = input;
		for (char& c : lowered) c = std::tolower(c);

		if (lowered == "init" && !bIsRunning)
		{
			if (bIsRunning)
			{
				std::cout << "Cannot init, already running an instance" << std::endl;
				continue;
			}
			engine = new LunaticEngine();
			engine->Init();
			bIsInitialized = true;
		}

		if (lowered == "run")
		{
			if (!bIsInitialized)
			{
				std::cout << "Cannot run, not initialized yet" << std::endl;
				continue;
			}
			if (bIsRunning)
			{
				std::cout << "Cannot run, already running an instance" << std::endl;
				continue;
			}
			runThread = std::thread(&LunaticEngine::TestRun, engine);
			bIsRunning = true;
		}

		if (lowered == "close")
		{
			if (bIsRunning && runThread.joinable() || bIsInitialized)
			{
				if (runThread.joinable())
				{
					engine->shouldQuit = true;
					runThread.join();
				}
				if (bIsRunning)
				{
					engine->Cleanup();
				}
				delete engine;
				engine = nullptr;

				bIsInitialized = false;
				bIsRunning = false;
			}
			else
			{
				break;
			}
		}
	}
#endif
	return 0;
}
