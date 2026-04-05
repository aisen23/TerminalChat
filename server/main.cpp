#include "pch.h"
#include "Application.h"

int main()
{
	tc::log::init("server");
	tc::Application app;
	int exitCode = app.run();
	tc::log::info("Exit code: {}", exitCode);
	return exitCode;
}
