#pragma once
//Contains template for serialization
#include <vector>

template <typename T>
char* convert(T& input) {

	char* t = (char*)malloc(sizeof(input));
	memcpy(t, &input, sizeof(input));

	return t;
}


template<typename T>
char* convert(std::vector<T>& input) {

	int size_input = sizeof(T) * input.size();

	T* arr = input.data();

	char* t = (char*)malloc(size_input);

	memcpy(t, arr, size_input);

	return t;
}
