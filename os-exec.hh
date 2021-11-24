#ifndef __cpp_concepts
#error os-exec library requires concepts support. Use newer C++ standard!
#else
#include <cassert>
#include <concepts>
#include <iostream>
#include <string_view>
#include <system_error>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace os_exec
{
	template<typename T> concept CStringable = requires (T const& a) { { a.c_str() } -> std::convertible_to<char const*>; };
	template<typename T> concept Convertible_To_CStr = CStringable<T> || std::convertible_to<T, char const*>;

	struct Non_Zero_Exit_Code : std::error_category
	{
		char const* name() const noexcept override { return "Non_Zero_Exit_Code"; }
		std::string message(int value) const override { return "exit status " + std::to_string(value); }
	};

	struct Killed_By_Signal : std::error_category
	{
		char const* name() const noexcept override { return "Killed_By_Signal"; }
		std::string message(int value) const override { return "killed by signal " + std::to_string(value); }
	};

	struct Stopped_By_Signal : std::error_category
	{
		char const* name() const noexcept override { return "Stopped_By_Signal"; }
		std::string message(int value) const override { return "stopped by signal " + std::to_string(value); }
	};

	struct Unknown_Termination_Cause : std::error_category
	{
		char const* name() const noexcept override { return "Unknown_Termination_Cause"; }
		std::string message(int) const override { return "unknown termination cause"; }
	};

	static std::error_category const& non_zero_exit_code()         { static Non_Zero_Exit_Code         domain = {}; return domain; }
	static std::error_category const& killed_by_signal()           { static Killed_By_Signal           domain = {}; return domain; }
	static std::error_category const& stopped_by_signal()          { static Stopped_By_Signal          domain = {}; return domain; }
	static std::error_category const& unknown_termination_cause()  { static Unknown_Termination_Cause  domain = {}; return domain; }

	// Return value that is probably fine to use in shell context
	std::string shell_quote(std::string_view value)
	{
		// Based on
		// https://github.com/python/cpython/blob/975ac326ffe265e63a103014fd27e9d098fe7548/Lib/shlex.py#L325-L334
		using namespace std::string_view_literals;
		if (value.empty())
			return "''";

		auto const is_safe = std::all_of(value.cbegin(), value.cend(), [](char c) { return std::isalnum(c) || "@%+=:,./-_"sv.find(c) != std::string_view::npos; });
		if (is_safe)
			return { value.data(), value.size() };

		std::string retval;
		retval.reserve(value.size());
		retval += '\'';

		for (;;) if (auto quote = value.find('\''); quote != std::string_view::npos) {
			retval.append(value.data(), quote).append("\\'", 2);
			value.remove_prefix(quote+1);
		} else {
			break;
		}

		if (!value.empty()) retval.append(value.data(), value.size());
		return retval += '\'';
	}

	// Execute program with arguments
	[[nodiscard]]
	std::error_code run(char const* program, std::convertible_to<char const*> auto const& ...args)
	{
		// TODO Rethink current approach
		// Go instead of using 'p' family of exec functions tries to find a executable by itself
		// which gives it ability to provide nice error message without forking process and communication
		// between process and child. This is more compilecated for know, but may be better in long run

		struct Exec_Failure
		{
			int *value = nullptr;
			~Exec_Failure() { if (value != nullptr) munmap(value, sizeof(value)); }
		} exec_failure;

		if (auto const ptr = mmap(nullptr, sizeof(*exec_failure.value), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0); ptr != MAP_FAILED) {
			*(exec_failure.value = (decltype(exec_failure.value))ptr) = 0;
		} else {
			return std::make_error_code((std::errc)errno);
		}

		auto const pid = fork();
		if (pid < 0) {
			return std::make_error_code((std::errc)errno);
		} else if (pid == 0) {
			// ----------------- CHILD -----------------
			if (execlp(program, program, args..., NULL) < 0) {
				*exec_failure.value = errno;
				exit(1);
			}
			assert(0 && "unreachable: exec will either error or replace this code with new binary");
		}

		// ----------------- PARENT -----------------
		int wstatus;
		if (waitpid(pid, &wstatus, 0) < 0) {
			return std::make_error_code((std::errc)errno);
		}

		if (*exec_failure.value != 0)
			return std::make_error_code((std::errc)*exec_failure.value);

		if (WIFEXITED(wstatus)) {
			if (auto exit_code = WEXITSTATUS(wstatus); exit_code == 0) return {};
			else return { exit_code, os_exec::non_zero_exit_code() };
		}
		if (WIFSIGNALED(wstatus)) { return { WTERMSIG(wstatus), os_exec::killed_by_signal() }; };
		if (WIFSTOPPED(wstatus))  { return { WSTOPSIG(wstatus), os_exec::stopped_by_signal() }; };

		return { 1, os_exec::unknown_termination_cause() };
	}

	// Print command to be executed and execute that command
	[[nodiscard]]
	std::error_code run_echo(char const* program, std::convertible_to<char const*> auto const& ...args)
	{
		std::cout << os_exec::shell_quote(program);
		((void)(std::cout << ' ' << os_exec::shell_quote(args)), ...);
		std::cout << std::endl;
		return os_exec::run(program, args...);
	}

	constexpr inline char const* as_cstr(Convertible_To_CStr auto const& v)
	{
		if constexpr (std::convertible_to<decltype(v), char const*>) return v;
		else return v.c_str();
	}

	[[nodiscard]]
	inline auto run(Convertible_To_CStr auto const& program, Convertible_To_CStr auto const& ...args)
	{
		return os_exec::run(as_cstr(program), as_cstr(args)...);
	}

	[[nodiscard]]
	inline auto run_echo(Convertible_To_CStr auto const& program, Convertible_To_CStr auto const& ...args)
	{
		return os_exec::run_echo(as_cstr(program), as_cstr(args)...);
	}
}
#endif