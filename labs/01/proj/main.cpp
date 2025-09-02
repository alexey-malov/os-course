#include "lib.h"
#include <iostream>

int main()
{
	int a, b;
	std::cout << "Enter two integers: ";
	std::cin >> a >> b;

	int result = Add(a, b);
	std::cout << "Sum: " << result << std::endl;

	return 0;
}
