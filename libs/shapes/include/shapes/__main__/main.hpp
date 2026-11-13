// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>

namespace shapes::__main__ {
	// entry point of `python -m shapes`, called with sys.argv; the result is
	// the exit code of the interpreter. Writes to std::cout and std::cerr,
	// which the Python glue redirects to sys.stdout and sys.stderr
	int main(std::vector<std::string>& argv);
}  // namespace shapes::__main__
