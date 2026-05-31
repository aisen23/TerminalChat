#include "pch.h"
#include "Application.h"

int main(int argc, char* argv[])
{
	std::string instanceId = (argc > 1) ? argv[1] : "";

	tc::log::init(instanceId);
	tc::Application app;
	int exitCode = app.run(instanceId);
	tc::log::info("Exit code: {}", exitCode);
	tc::log::shutdown();
	return exitCode;
}
