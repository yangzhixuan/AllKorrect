#pragma once
#include <functional>

class Defer {
	std::function<void()> func;
public:
	Defer(std::function<void()> fun):func(fun){}
	~Defer(){
		func();
	}
};
