# os-exec

Simple library for calling external programs. It's intended to be simple replacement for `std::system` or `fork` & `exec` pattern uses.

- Requires C++20
- Header only
- Linux only

## What library provides?

Two main functions:
- `os_exec::run(program, args...)` - execute program with given arguments
- `os_exec::run_echo(program, args...)` - execute program with given arguments, but before it, print what you are about to execute

`Program` and `args` can be anything that is convertible to `char const*`, like `std::string`, `std::filesystem::path` or `char const*` itself

## Example

```cpp
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
```

## Inspired by

- C++ [std::system](https://en.cppreference.com/w/cpp/utility/program/system)
- Go [os/exec](https://pkg.go.dev/os/exec)
- Python [shlex](https://docs.python.org/3/library/shlex.html)
