//
// Copyright Copyright 2012, System Insights, Inc.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#pragma once

#include <list>
#include <string>

//-- Forward class declarations.
class Option
{
public:
	enum EType
	{
		eBoolean,
		eCharacter,
		eInteger,
		eList
	};

	//---- Constructors
	// For arguments which have no switch char but appear in a special order.
	Option(
		int order,
		const char *&charPtr,
		const char *usage,
		const char *argDesc,
		bool required = true);

	// For arguments which have no switch char but appear in a special order.
	Option(
		int order,
		int &intRef,
		const char *usage,
		const char *argDesc,
		bool required = true);

	// For the rest of the argumets as in a file list.
	Option(
		std::list<std::string> &list,
		const char *usage,
		const char *argDesc,
		bool required = true,
		bool expand = false);

	// Given an agument with a switch char ('-') <name>
	Option(
		const char *name,
		const char *&charPtr,
		const char *usage,
		const char *argDesc,
		bool required = false,
		bool ignoreCase = false);

	// Given an agument with a switch char ('-') <name>
	Option(
		const char *name,
		bool &boolRef,
		const char *usage,
		bool aArgument = false,
		const char *argDesc = nullptr,
		bool required = false,
		bool ignoreCase = false);

	// Given an agument with a switch char ('-') <name>
	Option(
		const char *name,
		int &intRef,
		const char *usage,
		const char *argDesc,
		bool required = false,
		bool ignoreCase = false);

	// Given an agument with a switch char ('-') <name>
	Option(
		const char *name,
		std::list<std::string> &list,
		const char *usage,
		const char *argDesc,
		bool required = false,
		bool expand = false,
		bool ignoreCase = false);

	// Accept member-wise copy
	// Option(const Option &aOption);

	//---- Destructor
	~Option();

	EType getType() const {
		return type_; }
	const char *getName() const {
		return name_; }
	const char *getUsage() const {
		return usage_; }
	const char *getArgDesc() const {
		return argDesc_; }
	int getOrder() const {
		return order_; }
	bool ignoreCase() const {
		return ignoreCase_; }
	bool hasArgument() const {
		return argument_; }
	bool hasSwitch() const 
	{ return switch_; }
	bool isRequired() const {
		return required_; }
	bool isSet() const {
		return isSet_; }

	const char *getCharPtr() const  {
		return charPtrPtr_ != 0 ? *charPtrPtr_ : 0; }
	bool getBool() const {
		return boolPtr_ != 0 ? *boolPtr_ : false; }
	int getInt() const {
		return intPtr_ != 0 ? *intPtr_ : -1; }
	const std::list<std::string> &getList() const {
		return *list_; }
	bool operator<(const Option &another) const;

	bool setValue(const char *aCp);

protected:
	void expandFiles(const char *fileName);

protected:
	const char  *name_;
	const char **charPtrPtr_;
	bool *boolPtr_;
	int *intPtr_;
	std::list<std::string>   *list_;
	EType type_;
	int order_;
	bool required_;
	bool argument_;
	bool ignoreCase_;
	bool switch_;
	const char *usage_;
	bool isSet_;
	bool expand_;
	const char *argDesc_;

	friend class OptionsList;
};


class OptionsList : public std::list<Option>
{
public:
	//---- Constructors
	OptionsList();
	OptionsList(Option *optionList[]);

	//---- Destructor
	virtual ~OptionsList();

	void addOption(Option &option);

	int parse(int &argc, const char **argv);
	void usage();
	void setOwnsOptions(bool flag = true) {
		ownsOptions_ = flag; }
	void append(Option *option) {
		push_back(*option); }

protected:
	void getArg(const char ** &argv, int &argc, Option *option, const char *aAt);
	bool find(const char *optName, Option *&option);
	bool find(int order, Option *&option);

protected:
	const char *program_;
	int  unswitched_;
	bool ownsOptions_;
};

// ------------------------ Inline Methods ------------------------------
// #include <Options.inl>



