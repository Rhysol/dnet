#pragma once

template <typename T>
class Singleton
{
public:
	Singleton(const Singleton &) = delete;
	void operator=(const Singleton &) = delete;
	virtual ~Singleton() {}

	static T &Instance()
	{
		static T instance;
		return instance;
	}

protected:
	Singleton() {};
};