#include "pch.h"
#include "Application.h"

int main()
{
	tc::log::init();
	tc::Application app;
	int exitCode = app.run();
	tc::log::info("Exit code: {}", exitCode);
	tc::log::shutdown();
	return exitCode;
}
