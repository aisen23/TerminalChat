#include "pch.h"
#include "Application.h"

int main()
{
	tc::Application app;
	int exitCode = app.run();
	std::println("Exit code: {}", exitCode);
	return exitCode;
}
