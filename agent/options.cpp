/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#define _CRT_SECURE_NO_WARNINGS 1

#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "options.hpp"

using namespace std;

#ifdef _WINDOWS
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

// For arguments which have no switch char but appear in a special order.
Option::Option(int aOrder, const char *&aCharPtr, const char *aUsage,
		 const char *aArgDesc, bool aRequired)
  : name_(NULL), charPtrPtr_(&aCharPtr), boolPtr_(NULL), intPtr_(NULL),
    list_(0), type_(eCharacter), order_(aOrder), required_(aRequired),
    argument_(false), ignoreCase_(false), switch_(false), usage_(aUsage),
    isSet_(false), expand_(false), argDesc_(aArgDesc)
{
}

// For arguments which have no switch char but appear in a special order.
Option::Option(int aOrder, int &aIntRef, const char *aUsage,
		 const char *aArgDesc, bool aRequired)
  : name_(NULL), charPtrPtr_(NULL), boolPtr_(NULL), intPtr_(&aIntRef), list_(0),
    type_(eInteger), order_(aOrder), required_(aRequired), argument_(false),
    ignoreCase_(false), switch_(false), usage_(aUsage), isSet_(false),
    expand_(false), argDesc_(aArgDesc)
{
}

// For the rest of the argumets as in a file list.
Option::Option(list<string> &aList, const char *aUsage,
		 const char *aArgDesc, bool aRequired, bool aExpand)
  : name_(NULL), charPtrPtr_(NULL), boolPtr_(NULL), intPtr_(NULL),
    list_(&aList), type_(eList), order_(-1), required_(aRequired),
    argument_(false), ignoreCase_(false), switch_(false), usage_(aUsage),
    isSet_(false), expand_(aExpand), argDesc_(aArgDesc)
{
}

// Given an agument with a switch char ('-') <name> 
Option::Option(const char *aName, const char *&aCharPtr, const char *aUsage,
		 const char *aArgDesc, bool aRequired, bool aIgnoreCase)
  : name_(aName), charPtrPtr_(&aCharPtr), boolPtr_(NULL), intPtr_(NULL),
    list_(NULL), type_(eCharacter), order_(-1), required_(aRequired),
    argument_(true), ignoreCase_(aIgnoreCase), switch_(true), usage_(aUsage),
    isSet_(false), expand_(false), argDesc_(aArgDesc)
{
}


// Given an agument with a switch char ('-') <name> 
Option::Option(const char *aName, bool &aBoolRef, const char *aUsage,
		 bool aArgument, const char *aArgDesc, bool aRequired,
		 bool aIgnoreCase)
  : name_(aName), charPtrPtr_(NULL), boolPtr_(&aBoolRef), intPtr_(NULL),
    list_(NULL), type_(eBoolean), order_(-1), required_(aRequired),
    argument_(aArgument), ignoreCase_(aIgnoreCase), switch_(true),
    usage_(aUsage), isSet_(false), expand_(false), argDesc_(aArgDesc)
{
}

// Given an agument with a switch char ('-') <name> 
Option::Option(const char *aName, int &aIntRef, const char *aUsage,
		 const char *aArgDesc, bool aRequired, bool aIgnoreCase)
  : name_(aName), charPtrPtr_(NULL), boolPtr_(NULL), intPtr_(&aIntRef),
    list_(NULL), type_(eInteger), order_(-1), required_(aRequired),
    argument_(true), ignoreCase_(aIgnoreCase), switch_(true), usage_(aUsage),
    isSet_(false), expand_(false), argDesc_(aArgDesc)
{
}

// Given an agument with a switch char ('-') <name> 
Option::Option(const char *aName, list<string> &aList, const char *aUsage,
		 const char *aArgDesc, bool aRequired, bool aExpand,
		 bool aIgnoreCase)
  : name_(aName), charPtrPtr_(NULL), boolPtr_(NULL), intPtr_(NULL),
    list_(&aList), type_(eList), order_(-1), required_(aRequired),
    argument_(true), ignoreCase_(aIgnoreCase), switch_(true), usage_(aUsage),
    isSet_(false), expand_(aExpand), argDesc_(aArgDesc)
{
}

bool Option::setValue(const char *aCp)
{
  bool rc = true;

  if (type_ != eList && isSet_)
  {
    if (name_ != NULL)
      cerr << "Option " << name_ << " is already specified" << endl;
    else
      cerr << "Option is alread specified" << endl;
    return false;
  }
		
  switch (type_)
  {
  case eInteger:
    if (isdigit(*aCp))
      *(intPtr_) = atoi(aCp);
    else
      rc = false;
    break;
		
  case eBoolean:
    *(boolPtr_) = (*aCp == 'Y' || *aCp == 'T') ? true : false;
    break;

  case eCharacter:
    *(charPtrPtr_) = aCp;
    break;

  case eList:
    if (expand_)
      expandFiles(aCp);
    else
      list_->push_back(aCp);
    break;
		
  default:
    cerr << "Bad option type" << endl;
    rc = false;
  }

  if (rc)
    isSet_ = true;
	
  return rc;
}

void Option::expandFiles(const char *aFileName)
{
  if (strchr(aFileName, '*') != NULL || strchr(aFileName, '?') != NULL)
  {
#ifdef NOT_DEF
    WIN32_FIND_DATA fileData;
    HANDLE search = FindFirstFile(aFileName, &fileData);
    if (search != INVALID_HANDLE_VALUE)
    {
      bool done = false;
      while (!done)
      {
	char *cp = new char[strlen(fileData.cFileName) + 1];
	strcpy(cp, fileData.cFileName);
	list_->append(cp);
				
	done = !FindNextFile(search, &fileData);
      }
			
      FindClose(search);
    }
#endif
  }
  else
  {
    list_->push_back(aFileName);
  }
}

bool Option::operator<(Option &aOther)
{
  if (name_ == 0 && aOther.getName() != 0)
    return false;
  else if (name_ != 0 && aOther.getName() == 0)
    return true;
  else if (name_ == 0 && aOther.getName() == 0)
  {
    if (order_ == -1)
      return false;
    if (aOther.getOrder() == -1)
      return true;
    
    return order_ < aOther.getOrder();
  }
  
  return strcmp(name_, aOther.getName()) < 0;
}


// Accept member-wise copy
//Option::Option(const Option &aOption)

//---- Destructor
Option::~Option()
{
}

// --------- Class OptionList begins here.

OptionsList::OptionsList()
{
  unswitched_ = -1;
  ownsOptions_ = false;
  program_ = NULL;
}

OptionsList::OptionsList(Option *aOptionList[])
{
  program_ = NULL;
  unswitched_ = -1;
  ownsOptions_ = false;
  int i = 0;
  while (aOptionList[i] != 0)
  {
    push_back(*aOptionList[i++]);
  }
}

OptionsList::~OptionsList()
{
}

	
void OptionsList::addOption(Option &aOption)
{
  push_back(aOption);
}


int OptionsList::parse(int &aArgc, const char **aArgv)
{
  sort();
  
  // This assumes that the first option (app name) has already been removed.
  
  int order = 0, count = 0;

  program_ = "agent";
	
  Option *opt;
  const char **argp = aArgv;
  const char *cp;
	
  while (aArgc > 0)
  {
    if (**argp == '-')
    {
      cp = (*argp) + 1;
      bool next = false;

      while (*cp != 0 && !next)
      {
	if (find(cp, opt))
	{
	  count++;

	  if (opt->hasArgument())
	  {
	    getArg(argp, aArgc, opt, cp);
	    next = true;
	  }
	  else
	  {
	    if (opt->type_ != Option::eBoolean)
	    {
	      cerr << "Bad argument definition: " << opt->getName() << endl;
	    }
	    else if (opt->isSet_)
	    {
	      cerr << "Option " << opt->getName() << " is already specified" << endl;
	      usage();
	    }
	    else
	    {
	      *(opt->boolPtr_) = true;
	    }
						
	    cp += strlen(opt->getName());
	  }

	  opt->isSet_ = true;
	}
	else
	{
	  cerr << "Bad argument:" << *argp << endl;
	  usage();
	}
      }
    }
    else
    {
      if (find(order, opt))
      {
	if (!opt->setValue(*argp))
	  usage();

	count++;
      }
      else if (find(-1, opt))
      {
	if (!opt->setValue(*argp))
	  usage();

	count++;
      }

      order++;
    }
		
    argp++;
    aArgc--;
  }

  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    opt = &(*iter);
    if (opt->isRequired() && !opt->isSet())
    {
      if (opt->getName() != NULL)
      	cerr << "Required option -" << opt->getName() << " is not specified" << endl;
      else
      	cerr << "Required option <" << opt->getArgDesc() << "> is not specified" << endl;
      usage();
    }
  }
  
  
  return count;
}

void OptionsList::getArg(const char **&aArgv, int &aArgc, Option *aOpt, const char *aAt)
{
  const char *cp = aAt + strlen(aOpt->getName());
		
  if (*cp == '\0')
  {
    ++aArgv;
    cp = *aArgv;
    aArgc--;
		
    if (aArgc < 1 || *cp == '-')
    {
      cerr << "Argument required for -" << aOpt->getName() << endl;
      usage();
    }
  }

  if (!aOpt->setValue(cp))
    usage();
}

void OptionsList::usage()
{
  size_t len;
  char buffer[1024], *cp;

  cp = buffer;
  len = sprintf(buffer, "Usage: %s ", program_);
  cp += len;

  bool hasSimpleFlags = false;
	
  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    Option *opt = &(*iter);
    if (opt->getName() != NULL &&
        !opt->hasArgument() &&
        (len = strlen(opt->getName())) == 1)
    {
      hasSimpleFlags = true;
      break;
    }
  }

  if (hasSimpleFlags)
  {
    *cp++ = '[';
    *cp++ = '-';
		
    for (list<Option>::iterator iter = begin(); iter != end(); iter++)
    {
      Option *opt = &(*iter);
      if (opt->getName() != NULL &&
          !opt->hasArgument() &&
          (len = strlen(opt->getName())) == 1)
      {
	strcpy(cp, opt->getName());
	cp += len;
      }
    }
		
    *cp++ = ']';
  }

  char staging[128], *cp2;
  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    Option *opt = &(*iter);

    if (opt->getName() != NULL && !opt->hasArgument() && (len = strlen(opt->getName())) == 1)
      continue;
		
    *cp++ = ' ';

    cp2 = staging;
    if (!opt->isRequired())
    {
      *cp2++ = '[';
    }

    if (opt->getType() == Option::eList)
    {
      *cp2++ = '{';
    }
				
    if (opt->getName() != NULL && !opt->hasArgument() && strlen(opt->getName()) > 1)
    {
      len = sprintf(cp2, "-%s", opt->getName());
      cp2 += len;
    }
    else if (opt->getName() != NULL && opt->hasArgument())
    {
      len = sprintf(cp2, "-%s <%s>", opt->getName(), opt->getArgDesc());
      cp2 += len;
    }
    else if (opt->getName() == NULL)
    {
      len = sprintf(cp2, "<%s>", opt->getArgDesc());
      cp2 += len;
    }
		
    if (opt->getType() == Option::eList)
    {
      *cp2++ = '}';
      *cp2++ = '.';
      *cp2++ = '.';
      *cp2++ = '.';
    }
				
    if (!opt->isRequired())
    {
      *cp2++ = ']';
    }

    *cp2 = '\0';
		
    if (((cp2 - staging) + (cp - buffer)) > 79)
    {
      *cp++ = '\n';
      *cp = '\0';
			
      fputs(buffer, stderr);
      strcpy(buffer, "        ");
      cp = buffer + 8;
    }
		
    strcpy(cp, staging);
    cp += cp2 - staging;
  }

  *cp++ = '\n';
  *cp = '\0';
  fputs(buffer, stderr);
	

  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    Option *opt = &(*iter);
    if (opt->getName() != NULL)
    {
      if (opt->hasArgument())
	sprintf(buffer, "-%-2.2s <%s>", opt->getName(), opt->getArgDesc());
      else
	sprintf(buffer, "-%-6.6s", opt->getName());
    }
    else if (opt->getOrder() >= 0)
    {
      sprintf(buffer, "<%s>", opt->getArgDesc());
    }
    else
    {
      sprintf(buffer, "<%s>...", opt->getArgDesc());
    }
		
    fprintf(stderr, "    %-20.20s : ", buffer);
    const char *cp = opt->getUsage();
    while (*cp != '\0')
    {
      if (*cp == '\n')
      {
	fputc(*cp, stderr);
	int count = 4 + 20 + 1;
	while (count--)
	  fputc(' ', stderr);
				
	fputc('>', stderr);
	fputc(' ', stderr);
      }
      else
      {
	fputc(*cp, stderr);
      }
			
      cp++;
    }
    fputc('\n', stderr);
  }

  exit(256);
}

bool OptionsList::find(const char *aOpt, Option *&aOption)
{
  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    aOption = &(*iter);
    const char *name = aOption->getName();

    // Unnamed options are at the end of the list.
    if (name == 0)
      break;
				
    size_t len = strlen(name);
		
    if (aOption->ignoreCase())
    {
      if (strncasecmp(aOpt, name, len) == 0)
      {
	return true;
      }
    }
    else if (strncmp(aOpt, name, len) == 0)
    {
      return true;
    }
  }

  return false;
}

bool OptionsList::find(int aOrder, Option *&aOption)
{
  for (list<Option>::iterator iter = begin(); iter != end(); iter++)
  {
    aOption = &(*iter);

    // Unnamed options are at the end of the list.
    if (aOption->getName() == 0 && aOption->getOrder() == aOrder)
      return true;
  }

  return false;
}

