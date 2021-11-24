#include <filesystem>
#include <iostream>

#include <os-exec.hh>

int main()
{
	// simplest use
	{
		if (os_exec::run_echo("ls", "-la"))
			std::cerr << "ls failed\n";
	}

	// more precise error handling
	{
		auto const error = os_exec::run("echo", std::filesystem::current_path());

		if (!error) {
			std::cout << "Success!\n";
			return 0;
		}

		if (error.category() == os_exec::non_zero_exit_code()) {
			std::cout << "Exited with value: " << error.value() << '\n';
		} else {
			std::cout << "Error: " << error.message() << '\n';
		}
	}
}
