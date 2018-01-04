#pragma once

class ArgDesc {
public:
	std::string name;
	std::string description;
	std::string default_value; // not necessarily a string, this is just a representation

public:
	ArgDesc(const std::string &name = "", const std::string &default_value = "", const std::string &description = "")
		:name(name)
		, description(description)
		, default_value(default_value) {}

	ArgDesc(const ArgDesc &rhs) {
		name = rhs.name;
		description = rhs.description;
		default_value = rhs.default_value;
	}
};

typedef std::vector<std::shared_ptr<ArgDesc>> ARGLIST;

class FuncDesc {
public:
	std::string name;
	std::string category;
	std::string description;
	ARGLIST arguments;

	/** 
	 * this ID is assigned when we call xlfRegister, and we need to keep
	 * it around to call xlfUnregister if we are rebuilding the functions.
	 */
	int32_t registerID;

public:
	FuncDesc(const std::string &name = "", const std::string &category = "", const std::string &description = "", const ARGLIST &args = {})
		: name(name)
		, category(category)
		, description(description)
		, registerID(0)
	{
		for (auto arg : args) arguments.push_back(arg);
		std::cout << "fd ctro" << std::endl;
	}

	~FuncDesc() {
		std::cout << "~fd" << std::endl;
	}

	FuncDesc(const FuncDesc &rhs) {
		name = rhs.name;
		category = rhs.category;
		description = rhs.description;
		registerID = rhs.registerID;
		for (auto arg : rhs.arguments) arguments.push_back(arg);
		std::cout << "fd cc" << std::endl;
	}
};

typedef std::vector<std::shared_ptr<FuncDesc>> FUNCLIST;

